#!/bin/sh

echo "Changing state..."

cd $(dirname $0)
cd ../../
process=`pwd | xargs -i basename {}`

MCD_PIDS=`ps -ef | grep "./${process}_mcd ../etc/${process}_mcd.conf" | grep -v grep | awk '{print $2}'`
 
ret=0
for MAIN_PID in $MCD_PIDS
do
    kill -s USR1 $MAIN_PID
    let ret=ret+$?
done

if [ $ret -ne 0 ]
then
echo "change failed."
exit 1
else
echo "change ok."
exit 0
fi
