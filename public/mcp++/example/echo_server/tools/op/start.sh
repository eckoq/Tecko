#!/bin/sh

dir_exe=../cron

ulimit -c unlimited

# Enter ../cron
cd $(dirname $0)
cd $dir_exe
if [ $? -ne 0 ]; then 
    echo "cannot change dir to $dir_exe"
    exit 1  
fi

sh check_watchdog.sh

exit 0
