#!/bin/bash

# BEGIN_COMMON_COPYRIGHT_HEADER
# (c)LGPL2+
#
#
# Copyright: 2012-2013 Boomaga team https://github.com/Boomaga
# Authors:
#   Alexander Sokoloff <sokoloff.a@gmail.com>
#
# This program or library is free software; you can redistribute it
# and/or modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General
# Public License along with this library; if not, write to the
# Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301 USA
#
# END_COMMON_COPYRIGHT_HEADER

# Verifies that every C++ source file carries a licence marker compatible with
# the project: BSD 3-Clause, or LGPL v2.1 or later.
#
# Exits non-zero when a file is non-compliant, so it can gate CI. Upstream's
# version only printed colours and always succeeded, which meant a file could
# lose its copyright header without anyone noticing.
#
#   ./scripts/checkLicense.sh [dir]      # defaults to the whole repository
#   ALL=1 ./scripts/checkLicense.sh      # also list the compliant files

set -u

DIR="${1:-$(git rev-parse --show-toplevel 2>/dev/null || echo .)}"

if [ -t 1 ]; then
    RED='\E[0;31m'
    GREEN='\e[0;32m'
    NORM='\E[0m'
    RED_BG='\E[41m'
else
    RED=''; GREEN=''; NORM=''; RED_BG=''
fi

failed=0
checked=0

# git ls-files keeps generated build trees out of the results.
while read -r file; do
    [ -f "$file" ] || continue
    checked=$((checked + 1))

    license=$(head -n 5 "$file" | grep '(c)' | sed -e 's/*//' -e 's/^[[:space:]]*//')

    case "$license" in
        *LGPL2+*|*LGPL3+*|*DWTFYW*|*BSD*)
            [ -n "${ALL:-}" ] && printf "${GREEN}%-20s %s${NORM}\n" "$license" "$file"
            continue
            ;;

        *GPL2*|*GPL3*|*LGPL2*|*LGPL3*)
            colour=$RED
            ;;

        *)
            colour=$RED_BG
            [ -z "$license" ] && license='Not set'
            ;;
    esac

    printf "${colour}%-20s %s${NORM}\n" "$license" "$file"
    failed=$((failed + 1))
done < <(git ls-files -- "${DIR}" | grep -E '\.(cpp|h)$')

if [ "$failed" -gt 0 ]; then
    echo
    echo "${failed} of ${checked} files have a missing or incompatible licence header."
    exit 1
fi

echo "All ${checked} C++ files carry a compatible licence header."
