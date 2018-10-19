include ../incl_head.mk

INCLUDE += -I../ccd/ -I../dcc/
#定义_DEBUG_宏则会打印详细的日志到标准错误输出
#CFLAGS += -D_DEBUG_
#定义_SHMMEM_ALLOC_宏则会启用基于共享内存的内存分配器来传输业务数据
CFLAGS += -D_SHMMEM_ALLOC_
#定义_FASTMEM_DEBUG则会打印内存池的统计信息
#CFLAGS += -D_FASTMEM_DEBUG
OBJ = ../old/tfc_ipc_sv.o \
    ../old/tfc_error.o \
	../base/tfc_base_config_file.o \
	../watchdog/tfc_base_watchdog_client.o	\
	../wtg/wtg_client_api.o	\
	../ccd/tfc_net_cconn.o \
	../ccd/tfc_net_ipc_mq.o \
	../ccd/tfc_net_open_mq.o \
	../ccd/tfc_net_open_shmalloc.o	\
	../ccd/tfc_net_raw_cache.o \
	../ccd/tfc_net_socket_tcp.o \
	../ccd/mydaemon.o		\
	../ccd/myalloc.o	\
	../ccd/fastmem.o	\
	../ccd/dev_info.o	\
	tfc_net_dcc_main.o	\
	version.o

BIN = dcc asn_complete.so
all: $(BIN)
dcc: $(OBJ)
	$(CXX) $(CFLAGS) -o $@ $^ $(LIB)

#由于64位系统下需要加上-fPIC编译动态库才能通过，所以这里要区分32位和64位下的编译方法
ifeq ($(X86_64),0)
asn_complete.so: asn_complete.o
	$(CXX) $(CFLAGS) -o $@ $^ -shared 
else
asn_complete.so: asn_complete.o
	$(CXX) $(CFLAGS) -o $@ asn_complete.cpp -fPIC -shared 
endif	

include ../incl_tail.mk
