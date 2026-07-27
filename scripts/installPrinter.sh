#!/bin/sh

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

if ! [ $(id -u) = 0 ]; then
   echo "The script need to be run as root." >&2
   exit 1
fi



SCHEME="plicapage"
URI="${SCHEME}:/"
NAME="PlicaPage"
PPD="lsb/usr/plicapage/${SCHEME}.ppd"


while [ $# -gt 0 ]; do
  case $1 in
    -f|--force)
      FORCE=yes
      shift
      ;;  

  *)
      shift
      ;;  

  esac
done  


if [ -z "${FORCE}" ]
then
  if [ -n "$(LC_ALL=C lpstat -v 2>/dev/null | grep "$URI")" ] 
  then
    echo "Looks like ${URI} already installed. Use --force option." >&2
    exit 1
  fi
fi

printer=${NAME}
while $(LC_ALL=C lpstat -v 2>/dev/null | cut -d ':' -f 1 | cut -d ' ' -f 3 | grep -q "^${printer}"\$)
do
  number=$(($number + 1))
  printer="${NAME}-${number}"
done

pageSize="$(LC_ALL=C paperconf 2>/dev/null)" || pageSize=a4


# No -h localhost. That routes the request over IPP, which demands an
# authenticated CUPS user; root is not one, so under sudo it fails with
# "lpadmin: Unauthorized". Omitting it uses the local domain socket, where
# cupsd authorises root by its peer credentials.
if ! lpadmin -p "${printer}" -v ${URI} -E -m ${PPD} \
             -o printer-is-shared=no -o PageSize=${pageSize}
then
  echo "Failed to create the printer queue." >&2
  exit 1
fi

if [ -z "$(LC_ALL=C lpstat -d 2>/dev/null | grep 'system default destination:')" ]
then
  lpadmin -d "${printer}"
fi

# lpadmin can report success and still not produce a usable queue, so confirm.
if ! LC_ALL=C lpstat -v 2>/dev/null | grep -q "${URI}"
then
  echo "Printer ${printer} was not created." >&2
  exit 1
fi

echo "Printer ${printer} has been installed successfully."
exit 0
