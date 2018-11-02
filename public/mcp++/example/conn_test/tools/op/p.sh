#!/bin/sh                                                                                                                                                  
cd $(dirname $0)                                                                                                                                           
cd ../../                                                                                                                                                  
process=`pwd | xargs -i basename {}`
ps -flu ${LOGNAME} 2>/dev/null | awk '{ if( FNR == 1 ) printf "%s\n", $0;}'
ps -flu ${LOGNAME} 2>/dev/null | grep "./${process}_ccd" | grep -v grep | awk '{printf "%s\n", $0;}'
ps -flu ${LOGNAME} 2>/dev/null | grep "./${process}_mcd" | grep -v grep | awk '{printf "%s\n", $0;}'
ps -flu ${LOGNAME} 2>/dev/null | grep "./${process}_dcc" | grep -v grep | awk '{printf "%s\n", $0;}'
ps -flu ${LOGNAME} 2>/dev/null | grep "./${process}_watchdog" | grep -v grep | awk '{printf "%s\n", $0;}'

exit 0
