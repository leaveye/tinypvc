#!/bin/bash
if [ $# -lt 2 ]
then
    echo "Usage: $0 LOGFILE COUNT"
    exit 0
fi
LOGFILE="$1"
COUNT=$(( $2 + 0 ))

for i in `seq $COUNT`
do
    echo -e -n '\r'
    echo -e -n "round #$i: "
    ./testpvc &>$LOGFILE
    echo -e -n "ok"
done
echo -e "\rAll $COUNT rounds done."
