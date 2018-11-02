#!/bin/sh

SHM_IDS=`ipcs -m | awk '{if (match($1, /0x1122336/)) print $1}'`

for SHM_ID in $SHM_IDS; do
    echo "ipcrm -M $SHM_ID"
    ipcrm -M $SHM_ID 
done
