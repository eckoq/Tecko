#!/bin/sh

file_name=$1

while read line; do
    vid=$line    
    dstfile=$vid".f0.mp4"
    zone=$(echo $line|awk -F'_' '{print $1}')
    if [ $zone == "szg" ];then
        wget "http://bucketid1047-60001.sz.vod.tencent-cloud.com/$dstfile" -O /data/download/$dstfile
    elif [ $zone == "tjg" ];then
        wget "http://bucketid1047-60001.tj.vod.tencent-cloud.com/$dstfile" -O /data/download/$dstfile
    elif [ $zone == "shg" ];then
        wget "http://bucketid1047-60001.sh.vod.tencent-cloud.com/$dstfile" -O /data/download/$dstfile
    else
        echo "unkown zone"
    fi
done <$file_name
