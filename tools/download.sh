#!/bin/sh

file_name=$1
format_id=$2

while read line; do
    vid=$(echo $line |awk '{print $2}')
    dstfile=$vid".f$format_id"".mp4"
    zone=$(echo $vid|awk -F'_' '{print $1}')
    if [ $zone == "szg" ];then
        wget "http://bucketid1047-60001.sz.vod.tencent-cloud.com/$dstfile" -O /data/download/$dstfile
    elif [ $zone == "tjg" ];then
        wget "http://bucketid1047-60001.tj.vod.tencent-cloud.com/$dstfile" -O /data/download/$dstfile
    elif [ $zone == "shg" ];then
        wget "http://bucketid1047-60001.sh.vod.tencent-cloud.com/$dstfile" -O /data/download/$dstfile
    elif [ $zone == "1047" ];then
        wget "http://10.166.27.210/$dstfile?addrtype=2" -O /data/download/$dstfile
    else
        echo "unkown zone"
    fi
done <$file_name
