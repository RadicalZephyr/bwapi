#!/bin/sh

revNumber=0

if command -v git > /dev/null 2>&1; then
  # 2383 is the number of revisions that were cut when migrating to Github
  # Adding this value makes it more consistent with older releases
  revNumber=$((2383 + $(git rev-list HEAD --count)))
fi

{
  echo "#pragma once"
  echo "static const int SVN_REV = ${revNumber};"
  echo ""
  echo "#include \"starcraftver.h\""
} > include/svnrev.h
