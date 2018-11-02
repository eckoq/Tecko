include ../incl_head.mk

POPPY_SUPPORT = 0

INCLUDE +=		-I../dcc/. -I../common/. -I../

#定义_DEBUG_宏则会打印详细的日志到标准错误输出
#CFLAGS += -D_DEBUG_
#定义_SPEEDLIMIT_宏则会启用上传下载速率控制功能
CFLAGS += -D_SPEEDLIMIT_
#定义_SHMMEM_ALLOC_宏则会启用基于共享内存的内存分配器来传输业务数据
CFLAGS += -D_SHMMEM_ALLOC_
#定义_FASTMEM_DEBUG则会打印内存池的统计信息
#CFLAGS += -D_FASTMEM_DEBUG
#定义_OUTPUT_LOG_会打印日志到/var/log/messages
CFLAGS += -D _OUTPUT_LOG_

OBJ	= tfc_net_cconn.o \
	tfc_net_ipc_mq.o \
	tfc_net_open_mq.o \
	tfc_net_open_shmalloc.o	\
	tfc_net_raw_cache.o \
	tfc_net_socket_tcp.o \
	tfc_net_socket_udp.o	\
	tfc_net_ccd_main.o	\
	mydaemon.o			\
	version.o			\
	myalloc.o			\
	fastmem.o			\
	over_ctrl.o         \
	../common/log_client.o			\
	./app/compatible_ccd_net_handler.o \
	../common/stat.o

BIN	= ccd

ifeq ($(POPPY_SUPPORT),1)
BIN += poppy_complete.so
endif

all: $(BIN)
ccd: $(OBJ)
	$(CXX) $(CFLAGS) -o $@ $^ $(LIB)

ifeq ($(POPPY_SUPPORT),1)
poppy_complete.so:
	$(CXX) $(CFLAGS) -I../libhttp/. -fPIC -shared -o $@ poppy_complete.cpp -L../libhttp -lhttp
endif

include ../incl_tail.mk
