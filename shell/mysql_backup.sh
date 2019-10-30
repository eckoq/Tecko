#!/bin/sh

#db info
g_db_user="test"
g_db_passwd="test"
g_db_host="172.27.208.71"
g_db_name="QQAlbumbyWiazrds"

#backup conf
g_mysqldump="/usr/local/mysql/bin/mysqldump"
g_mysql="/usr/local/mysql/bin/mysql"
g_pre_backfile_name=$g_db_name"_back"
g_max_back_cnt=5

#judge backup dir
g_backup_dir="/tmp/mysql/backup"
if [ ! -d $g_backup_dir ];then
    mkdir -p $g_backup_dir
    echo "Mysql backup dir in $g_backup_dir"
fi


get_max_backup_num()
{
    tmpfile="/tmp/r.tmp"
    for back_file_name in `ls $g_backup_dir`; do
        echo "$back_file_name" >> $tmpfile
    done;
    
    max=$(awk -F'.' 'BEGIN{max=0}{if($2 > max) max=$2}END{print max}' $tmpfile)
    
    if [ -x $tmpfile ];then
        rm $tmpfile
    fi
    echo $max
}

backup_db()
{
    max_backup_num=$(get_max_backup_num)
    let 'max_backup_num=max_backup_num+1'
    $g_mysqldump -u $g_db_user -h $g_db_host\
                 -p$g_db_passwd $g_db_name >\
                 "$g_backup_dir/$g_pre_backfile_name.$max_backup_num"
}

#only allow g_max_back_cnt
#rm old file
check_backup_file_num()
{
    file_num=$(ls -t $g_backup_dir | wc -l)
    all_file_name=$(ls -t $g_backup_dir)
    new_file_name=$(ls -t $g_backup_dir | head -$g_max_back_cnt)
    for file in $all_file_name; do
        flag=0
        for new_file in $new_file_name; do
            if [ $file = $new_file ];then
                flag=1
            fi
        done

        if [ $flag -eq 0 ];then
            #rm old file
            rm "$g_backup_dir/$file"
        fi
    done
}

#main
backup_db
check_backup_file_num
