#!/bin/sh

echo "Stopping service..."
                                                                                                                                                           
cd $(dirname $0)
cd ../../                                                                                                                                                  
process=`pwd | xargs -i basename {}`
WATCHDOG_PIDS=`ps -ef | grep "./${process}_watchdog" | grep -v grep | awk '{print $2}'`

ret=0
for PID in $WATCHDOG_PIDS
do
    cmd="kill -15 $PID"
    echo $cmd
    $cmd
    let ret=ret+$?
done

if [ $ret -ne 0 ]
then
    echo "Can't stop."
    exit 1
else
    echo "Service stoped."
    exit 0
fi
