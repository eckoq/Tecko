#ccd,mcd,dcc makefile��ͬ�Ķ���
CC = gcc
CXX = g++

INCLUDE += -I../old/. -I../base/. -I../watchdog -I../wtg
LIB     += -ldl -lpthread -lrt -L../watchdog -lwatchdog -L../wtg -lwtg_api

CFLAGS  += -g -O2 -Wall -pipe -D_REENTRANT -D_GNU_SOURCE -rdynamic
# _MQ_OPT��ʾ�����ڴ�MQ�Ķ��Ż�������FIFO��д�ľ������
# CFLAGS += -D_MQ_OPT
# _CACHE_COMPLETE_CHECK��ʾmemcache��StartUp���������������
# CFLAGS += -D_CACHE_COMPLETE_CHECK
# ����_OUTPUT_LOG�����������־��/var/log/message
CFLAGS += -D_OUTPUT_LOG -fPIC

X86_64 = 0
SYSBIT = $(shell getconf LONG_BIT)
ifeq ($(SYSBIT),64)
	X86_64 = 1
endif	

