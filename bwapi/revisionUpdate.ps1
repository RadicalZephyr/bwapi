# Writes include/svnrev.h with a revision number derived from the git history.
#
# A PowerShell port of the revisionUpdate.vbs it replaces: VBScript is deprecated by Microsoft
# and scheduled for removal, and this script is on the build path of every project through
# SVNRevGen, so the build must not depend on it. Run from the bwapi/ directory.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File ./revisionUpdate.ps1

$ErrorActionPreference = 'Stop'

# 2383 is the number of revisions that were cut when migrating to GitHub. Adding this value
# makes the number consistent with the older releases.
$revNumber = 0
if (Get-Command git -ErrorAction SilentlyContinue) {
  $count = (& git rev-list HEAD --count 2>$null)
  if ($LASTEXITCODE -eq 0 -and $count) {
    $revNumber = 2383 + [int]($count.Trim())
  }
}

$contents = @"
#pragma once
static const int SVN_REV = $revNumber;

#include "starcraftver.h"

"@

$target = Join-Path $PSScriptRoot 'include/svnrev.h'
Set-Content -Path $target -Value $contents -Encoding ASCII -NoNewline
