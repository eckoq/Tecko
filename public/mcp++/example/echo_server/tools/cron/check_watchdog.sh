#!/bin/sh

cd $(dirname $0)
cd ../../
process=`pwd | xargs -i basename {}`"_watchdog"

cd ./bin

count=`ps -ef|grep "./${process} ../etc/${process}.conf"|grep -v grep|wc -l`

if [ $count -lt 1 ]; then
    ./${process}
elif [ $count -gt 1 ]; then
    for pid in `ps -ef |grep -v grep|grep ${process} |awk '{print $2}'`
    do
        kill -2 $pid
    done

    ./${process}
else
    echo "${process} is already running";
    exit 0;
fi
