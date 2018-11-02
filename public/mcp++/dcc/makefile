include ../incl_head.mk

INCLUDE += -I../ccd/ -I../dcc/ -I../common/. -I../
#定义_DEBUG_宏则会打印详细的日志到标准错误输出
#CFLAGS += -D_DEBUG_
#定义_SHMMEM_ALLOC_宏则会启用基于共享内存的内存分配器来传输业务数据
CFLAGS += -D_SHMMEM_ALLOC_
CFLAGS += -D_SPEEDLIMIT_
#定义_FASTMEM_DEBUG则会打印内存池的统计信息
#CFLAGS += -D_FASTMEM_DEBUG
#定义_OUTPUT_LOG_会打印日志到/var/log/messages
CFLAGS += -D_OUTPUT_LOG_
vpath %.cpp ../ccd:../common:./app
vpath %.c ../ccd

OBJ = tfc_net_cconn.o \
	tfc_net_ipc_mq.o \
	tfc_net_open_mq.o \
	tfc_net_open_shmalloc.o	\
	tfc_net_raw_cache.o \
	tfc_net_socket_tcp.o \
	tfc_net_socket_udp.o \
	over_ctrl.o\
	mydaemon.o		\
	myalloc.o	\
	fastmem.o	\
	tfc_net_dcc_main.o	\
	log_client.o  \
	./app/compatible_dcc_net_handler.o \
	stat.o \
	version.o

BIN = dcc
all: $(BIN)
dcc: $(OBJ)
	$(CXX) $(CFLAGS) -o $@ $^ $(LIB)

include ../incl_tail.mk
