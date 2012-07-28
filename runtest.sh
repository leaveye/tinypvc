#!/bin/bash
if [ $# -lt 3 ]
then
    echo "Usage: $0 APP COUNT LOGFILE"
    exit 0
fi
APP="$1"
COUNT=$(( $2 + 0 ))
LOGFILE="$3"

test -x "$APP" || exit 1

for i in `seq $COUNT`
do
    echo -e -n '\r'
    echo -e -n "round #$i: "
    eval "$APP &>$LOGFILE" || { echo failed; exit 1; }
    echo -e -n "ok"
done
echo -e "\rAll $COUNT rounds done."
