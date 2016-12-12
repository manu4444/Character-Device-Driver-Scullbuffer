#!/bin/sh

fullpath=$(realpath "$0")
usepath=$fullpath/scull.ko
echo "usepath first is $usepath"
if [ ! -f $usepath ]; then
    echo "changing file name.."
    usepath=${fullpath%/*}/scull.ko
fi

echo $usepath
/sbin/insmod "$usepath" $* || exit 1
