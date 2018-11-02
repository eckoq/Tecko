#ccd,mcd,dcc makefile共同的东西
CC = gcc
CXX = g++

INCLUDE += -I../old/. -I../base/. -I../watchdog
LIB     += -ldl -lpthread -lrt -L../base -ltfcbase -L../watchdog -lwatchdog

CFLAGS  += -g -O2 -Wall -pipe -D_REENTRANT -D_GNU_SOURCE -rdynamic

ifeq ($(coverage), yes)
CFLAGS  += -fprofile-arcs -ftest-coverage
LDFLAGS += -lgcov
endif

X86_64 = 1
ifeq ($(m32), yes)
CFLAGS += -m32 
LFLAGS += -m32
LDFLAGS += -m32
X86_64 = 0
endif

# _MQ_OPT表示共享内存MQ的读优化，减少FIFO读写的竞争情况
# CFLAGS += -D_MQ_OPT
# _CACHE_COMPLETE_CHECK表示memcache的StartUp数据完整检测启用
CFLAGS += -D_CACHE_COMPLETE_CHECK
