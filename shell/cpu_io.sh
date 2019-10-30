#!/bin/sh

cpu_info()
{
   
    load_average=`top -n 1  | sed -n '1p' | sed 's/,/:/g' |\
                  sed 's/ *//g' | awk -F':' '{for(i=NF-2; i<=NF; i++) print $i}'`

    load_str=""
    load_key=(one_min_load five_min_load fifteen_min_load)
    for i in {0..2}; do
        key=${load_key[i]}
        let "i+=1"
        value=$(echo "$load_average" | sed -n "$i p")
        load_str+="$key:$value  "
    done 

    cpu_stat=$(top -n 1 | sed -n '3p')
    
    echo "cpu info......"
    echo $load_str
    echo $cpu_stat
}

io_info()
{
    echo "io info......"
    iostat -x -d | sed -n '3,$p' 
}

g_cnt=0

while [ $g_cnt -lt 3 ]; do
    cpu_info
    echo ""
    io_info
    sleep 1
    let "g_cnt+=1"
done
