#!/bin/sh

FILE=$1

if [ "$FILE" = "" ]; then
	echo "Missing file operand"
	echo ""
	echo "Usage:  testBackend.sh PDF_FILE"
	exit 1
fi

FILE=$(readlink -e "${FILE}")


plicapagebackend=""

dir=$(pwd)
while [[ "$dir" != "/" ]]; do
	if [ -d "${dir}/.git" ]; then 
		plicapagebackend=$(find "$dir" -name "plicapagebackend")
		break
	fi

	dir=$(dirname $dir)
	sleep 1
done


if [ "$plicapagebackend" == "" ]; then
	plicapagebackend=$(find "/usr/local/lib" "/usr/lib" -name "plicapagebackend" 2>/dev/null)
fi

if [ "$plicapagebackend" == "" ]; then
	"echo plicapagebackend not found"
fi


echo "################"
echo "# program - $plicapagebackend"
echo "# file    - $FILE"
echo "#"


#               <jobId> <title> <count> <options> <user>
cat "${FILE}" | $plicapagebackend 123 	"title"	1 		""		  $USER 	