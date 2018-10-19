#ccd,mcd,dcc makefile共同的东西
CC = gcc
CXX = g++

INCLUDE += -I../old/. -I../base/. -I../watchdog -I../wtg
LIB     += -ldl -lpthread -lrt -L../watchdog -lwatchdog -L../wtg -lwtg_api

CFLAGS  += -g -O2 -Wall -pipe -D_REENTRANT -D_GNU_SOURCE -rdynamic
# _MQ_OPT表示共享内存MQ的读优化，减少FIFO读写的竞争情况
# CFLAGS += -D_MQ_OPT
# _CACHE_COMPLETE_CHECK表示memcache的StartUp数据完整检测启用
# CFLAGS += -D_CACHE_COMPLETE_CHECK
# 定义_OUTPUT_LOG则输出错误日志到/var/log/message
CFLAGS += -D_OUTPUT_LOG -fPIC

X86_64 = 0
SYSBIT = $(shell getconf LONG_BIT)
ifeq ($(SYSBIT),64)
	X86_64 = 1
endif	

