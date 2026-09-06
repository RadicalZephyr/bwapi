#!/usr/bin/env bash
#
# Provision a Linux container to build and run BWAPI's portable code under
# Clang's AddressSanitizer and UndefinedBehaviorSanitizer.
#
# Intended as the environment setup command for Claude Code on the web, but it
# is a plain script -- it runs the same way on any Ubuntu box.
#
# It is idempotent: re-running it on an already-provisioned container is cheap
# and harmless.
#
# Usage:
#   scripts/setup-sanitizers.sh            # x86_64 and i386 (default)
#   scripts/setup-sanitizers.sh --no-i386  # x86_64 only
#
set -euo pipefail

WANT_I386=1
for arg in "$@"; do
  case "$arg" in
    --no-i386) WANT_I386=0 ;;
    -h|--help) awk 'NR>1 && /^#/ { sub(/^# ?/, ""); print; next } NR>1 { exit }' "$0"; exit 0 ;;
    *) echo "setup-sanitizers: unknown argument: $arg" >&2; exit 2 ;;
  esac
done

log()  { printf '\033[1m==> %s\033[0m\n' "$*"; }
warn() { printf '\033[33mwarning: %s\033[0m\n' "$*" >&2; }
die()  { printf '\033[31merror: %s\033[0m\n' "$*" >&2; exit 1; }

SUDO=""
if [ "$(id -u)" -ne 0 ]; then
  command -v sudo >/dev/null 2>&1 || die "not root and sudo is unavailable"
  SUDO="sudo"
fi

export DEBIAN_FRONTEND=noninteractive

have_pkg() { dpkg-query -W -f='${Status}' "$1" 2>/dev/null | grep -q '^install ok installed$'; }

apt_update() {
  # Some base images carry third-party PPAs that the agent proxy blocks with a
  # 403. Those are not our packages, so their failure must not abort
  # provisioning. A package we genuinely need failing to resolve is still
  # caught -- by the install step, which does fail loudly.
  $SUDO apt-get update -qq 2>&1 | grep -viE 'ppa\.launchpad|deadsnakes|ondrej' >&2 || true
}

# --- 1. Locate the compiler, and derive its version rather than pinning one ---
# The base image's clang moves over time. Hardcoding a major version here is a
# landmine that surfaces months later as an unexplained link failure.

command -v clang++ >/dev/null 2>&1 || die "clang++ not found on PATH"
CLANG_VERSION="$(clang++ -dumpversion)"
CLANG_MAJOR="${CLANG_VERSION%%.*}"
log "clang++ ${CLANG_VERSION} (major ${CLANG_MAJOR})"

for tool in cmake ninja llvm-symbolizer; do
  command -v "$tool" >/dev/null 2>&1 \
    || warn "$tool not found on PATH; the sanitizer build will be degraded"
done

# --- 2. Install the sanitizer runtimes ---
# Ubuntu's clang ships without libclang_rt: -fsanitize=address compiles fine and
# then fails at link with "cannot find libclang_rt.asan-x86_64.a". Supplying
# that is the whole reason this script exists.
#
# Note this single package ships BOTH the x86_64 and the i386 runtime archives,
# so it covers 32-bit builds too. Do NOT be tempted to also install
# libclang-rt-N-dev:i386 to "add" 32-bit support: it is redundant, and apt
# resolves the resulting libc6-i386/libc6-amd64:i386 conflict by REMOVING the
# native package, leaving you with no x86_64 runtime at all.

RT_PKG="libclang-rt-${CLANG_MAJOR}-dev"

if have_pkg "$RT_PKG"; then
  log "$RT_PKG already installed"
else
  log "installing $RT_PKG (provides both x86_64 and i386 runtimes)"
  apt_update
  $SUDO apt-get install -y --no-install-recommends "$RT_PKG" \
    || die "could not install $RT_PKG (is clang ${CLANG_MAJOR} packaged for this release?)"
fi

# --- 3. Add the 32-bit C++ headers ---
# BWAPI ships as a Win32 (x86) DLL -- 555 of the solution's 773 build
# configurations are Win32. Sanitizing only x86_64 would exercise a word size
# the library is never actually built at, which is precisely where pointer-width
# assumptions hide. So 32-bit is on by default.
#
# The runtimes are already present from step 2; what is missing on a stock image
# is the 32-bit libstdc++ headers, without which -m32 fails at <cstdio>.

if [ "$WANT_I386" -eq 1 ]; then
  command -v g++ >/dev/null 2>&1 || die "g++ not found; needed to locate the libstdc++ headers"
  STDCXX_PKG="libstdc++-$(g++ -dumpversion | cut -d. -f1)-dev:i386"

  if have_pkg "$STDCXX_PKG"; then
    log "$STDCXX_PKG already installed"
  else
    log "installing $STDCXX_PKG (BWAPI ships as Win32/x86)"
    $SUDO dpkg --add-architecture i386
    apt_update
    if ! $SUDO apt-get install -y --no-install-recommends "$STDCXX_PKG"; then
      warn "32-bit C++ headers unavailable; continuing with x86_64 only."
      warn "Pass --no-i386 to skip this step and silence the warning."
      WANT_I386=0
    fi
  fi
fi

# --- 4. Prove the sanitizers actually fire ---
# Confirming a package installed is not the same as confirming that
# -fsanitize=address links and traps. Verify the real behaviour, so a broken
# container fails here, loudly, rather than halfway through a debugging session.

SMOKE_DIR="$(mktemp -d)"
trap 'rm -rf "$SMOKE_DIR"' EXIT

cat > "$SMOKE_DIR/asan.cpp" <<'EOF'
#include <cstdio>
int main() {
  int *p = new int[4];
  std::printf("%d\n", p[5]);          // heap-buffer-overflow
  delete[] p;
}
EOF

# Kept runtime-dependent on argc: a constant-folded overflow is diagnosed at
# compile time and would never exercise the runtime library we are testing.
cat > "$SMOKE_DIR/ubsan.cpp" <<'EOF'
int mul(int a, int b) { return a * b; }
int main(int argc, char **) {
  return mul(2000000000, 1 + argc) & 1;   // signed integer overflow
}
EOF

smoke() {
  local arch_flag="$1" label="$2" out rc

  out="$SMOKE_DIR/asan_$label"
  # shellcheck disable=SC2086  # arch_flag is intentionally word-split (empty or -m32)
  clang++ $arch_flag -std=c++17 -g -fsanitize=address -fno-omit-frame-pointer \
      "$SMOKE_DIR/asan.cpp" -o "$out" \
    || die "[$label] ASan failed to build or link"
  # Redirect to a file rather than piping into grep: the program is *meant* to
  # abort, and under `set -o pipefail` that nonzero status would fail the
  # pipeline even when grep matched.
  set +e
  ASAN_OPTIONS=detect_leaks=0 "$out" >"$SMOKE_DIR/as.log" 2>&1
  rc=$?
  set -e
  grep -q 'heap-buffer-overflow' "$SMOKE_DIR/as.log" \
    || die "[$label] ASan linked but did not report a known heap-buffer-overflow"
  [ "$rc" -ne 0 ] \
    || die "[$label] ASan reported but did not abort"
  log "[$label] ASan reports heap-buffer-overflow and aborts"

  out="$SMOKE_DIR/ubsan_$label"
  # shellcheck disable=SC2086
  clang++ $arch_flag -std=c++17 -g -fsanitize=undefined -fno-sanitize-recover=all \
      -fno-omit-frame-pointer "$SMOKE_DIR/ubsan.cpp" -o "$out" \
    || die "[$label] UBSan failed to build or link"
  set +e
  UBSAN_OPTIONS=print_stacktrace=1 "$out" >"$SMOKE_DIR/ub.log" 2>&1
  rc=$?
  set -e
  grep -q 'signed integer overflow' "$SMOKE_DIR/ub.log" \
    || die "[$label] UBSan linked but did not report a known signed overflow"
  [ "$rc" -ne 0 ] \
    || die "[$label] UBSan reported but did not abort; -fno-sanitize-recover is not in effect"
  log "[$label] UBSan reports signed integer overflow and aborts"
}

log "verifying sanitizers against known faults"
smoke "" "x86_64"
if [ "$WANT_I386" -eq 1 ]; then smoke "-m32" "i386"; fi

# --- 5. Report what the caller now has ---

BOLD=$(printf '\033[1m'); OFF=$(printf '\033[0m')
cat <<EOF

${BOLD}Sanitizer toolchain ready.${OFF}

  clang++      $(command -v clang++)  (${CLANG_VERSION})
  runtimes     x86_64$([ "$WANT_I386" -eq 1 ] && echo ' + i386 (-m32)')
  symbolizer   $(command -v llvm-symbolizer 2>/dev/null || echo 'MISSING - stack traces will not be symbolized')

Compile flags:
  -fsanitize=address,undefined -fno-sanitize-recover=all
  -fno-omit-frame-pointer -g -O1

Runtime options:
  export ASAN_OPTIONS=detect_leaks=1:detect_stack_use_after_return=1:abort_on_error=1
  export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1

BWAPI has no Linux build yet, so this provisions the toolchain only; it does not
configure or build the project.
EOF
