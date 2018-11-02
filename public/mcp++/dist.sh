#!/bin/sh

DIST=DIST
SRC_DIR=.

function package() {
    M32="yes"
    if [ "$1" == "m32" ];then
        PKG_DIR=${DIST}/mcp++_x86
        M32="yes"
    elif [ "$1" == "m64" ]; then
        PKG_DIR=${DIST}/mcp++_x86_64
        M32="no"
    else
        echo "usage: $0 [m32|m64]"
        exit 1
    fi
    
    rm -fr ${PKG_DIR}
    make m32=${M32};
    
    BIN_DIR=$PKG_DIR/bin
    LIB_DIR=$PKG_DIR/lib
    HDR_DIR=$PKG_DIR/inc
    TOOLS_DIR=$PKG_DIR/tools
    
    mkdir -p $BIN_DIR
    mkdir -p $LIB_DIR
    mkdir -p $HDR_DIR/{base,ccd,dcc,libhttp,mcd,old,watchdog,common}
    mkdir -p $TOOLS_DIR
    
    alias cp="cp -f "
    
    cp $SRC_DIR/ccd/ccd $BIN_DIR/ccd
    cp $SRC_DIR/dcc/dcc $BIN_DIR/dcc
    cp $SRC_DIR/mcd/mcd $BIN_DIR/mcd
    cp $SRC_DIR/watchdog/watchdog $BIN_DIR/watchdog
    
    cp $SRC_DIR/tools/so_loader/so_loader $TOOLS_DIR
    cp $SRC_DIR/tools/get_mq_stat/get_mq_stat $TOOLS_DIR
    cp $SRC_DIR/tools/mcp_trace/mcp_trace $TOOLS_DIR
    cp $SRC_DIR/common/remove_log_client_shm $TOOLS_DIR
    cp $SRC_DIR/tools/get_so_info/get_so_info $TOOLS_DIR
    
    cp $SRC_DIR/base/libtfcbase.a $LIB_DIR
    cp $SRC_DIR/base/libmcptest.a $LIB_DIR
    cp $SRC_DIR/base/asn13_complete.so $LIB_DIR
    cp $SRC_DIR/base/asn20_complete.so $LIB_DIR
    cp $SRC_DIR/libhttp/libhttp.a $LIB_DIR
    cp $SRC_DIR/libhttp/libhttpcxx.a $LIB_DIR
    cp $SRC_DIR/libhttp/http_complete.so $LIB_DIR
    cp $SRC_DIR/watchdog/libwatchdog.a $LIB_DIR
    
    cp $SRC_DIR/base/*.h* $HDR_DIR/base
    cp $SRC_DIR/ccd/*.h*  $HDR_DIR/ccd
    cp $SRC_DIR/dcc/*.h*  $HDR_DIR/dcc
    cp $SRC_DIR/libhttp/*.h*  $HDR_DIR/libhttp
    cp $SRC_DIR/mcd/*.h*  $HDR_DIR/mcd
    cp $SRC_DIR/watchdog/*.h* $HDR_DIR/watchdog
    cp $SRC_DIR/watchdog/watchdog.conf $HDR_DIR/watchdog
    cp $SRC_DIR/common/*.h* $HDR_DIR/common
    
    
    mkdir -p $HDR_DIR/mcd/inc
    mkdir -p $HDR_DIR/mcd/app
    mkdir -p $HDR_DIR/ccd/app
    mkdir -p $HDR_DIR/dcc/app
    
    cp $SRC_DIR/mcd/inc/*.h* $HDR_DIR/mcd/inc/
    cp $SRC_DIR/mcd/app/*.h $HDR_DIR/mcd/app
    cp $SRC_DIR/ccd/app/*.h $HDR_DIR/ccd/app
    cp $SRC_DIR/dcc/app/*.h $HDR_DIR/dcc/app
    cp $SRC_DIR/old/*.h* $HDR_DIR/old
}

# 1. clear DIST
if [ ! -d ${DIST} ]
then
    mkdir ${DIST}
fi

rm -rf ${DIST}/*

# 2. prepare m32, m64
package m32
if [ $? -ne 0 ]
then
    echo "package m32 failed"
fi

package m64
if [ $? -ne 0 ]
then
    echo "package m64 failed"
fi

# 3. copy example code
cp -r ${SRC_DIR}/example ${DIST}/example

# 4. tar distribute package
version=`./${DIST}/mcp++_x86_64/bin/ccd -v|grep "mcp++"|awk '{print $3}'`
date=`./${DIST}/mcp++_x86_64/bin/ccd -v|grep "date"|awk -F ": " '{print $2}'`
unix_time=`date +%s -d "${date} 00:00:00"`
day=`date +"%Y%m%d" -d@$unix_time`

TARGET="mcp++_${version}_${day}"
rm -rf ${TARGET}
mv ${DIST} ${TARGET}
tar -zcvf ${TARGET}.tar.gz ${TARGET}
