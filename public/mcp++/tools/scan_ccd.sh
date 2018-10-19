ip=`/sbin/ifconfig |grep -A 1 eth1|grep 'inet addr'|awk '{print $2}'|awk -F':' '{print $2}'`
pids=`ps -ef|grep ccd|grep -v grep|awk '{print $2}'`
if [[ -n $pids ]]
then	
	for pid in $pids
	do
		exe=`readlink /proc/$pid/exe`
        if [[ -f $exe ]]
		then	
			ver=`strings $exe | egrep "mcp\-version :|mcp\+\+ :|version   :"`
        	bdate=`strings $exe | grep "build date"`
			echo $ip"#"$exe"#"$ver"#"$bdate
		fi
	done
else
	echo $ip"#NONE#"
fi	
