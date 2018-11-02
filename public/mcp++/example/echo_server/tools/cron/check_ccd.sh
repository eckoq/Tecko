#!/bin/sh

cd $(dirname $0)
cd ../../
process=`pwd | xargs -i basename {}`"_ccd"

cd ./bin

count=`ps -ef|grep "./${process} ../etc/${process}.conf"|grep -v grep|wc -l`

if [ $count -lt 1 ]; then
    ./${process} ../etc/${process}.conf
elif [ $count -gt 1 ]; then
    for pid in `ps -ef |grep -v grep|grep ${process} |awk '{print $2}'`
    do
        kill -2 $pid
    done

    ./${process} ../etc/${process}.conf
else
    exit 0;
fi
