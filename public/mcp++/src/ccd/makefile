include ../incl_head.mk

INCLUDE +=		-I../dcc/.

#定义_DEBUG_宏则会打印详细的日志到标准错误输出
#CFLAGS += -D_DEBUG_
#定义_SPEEDLIMIT_宏则会启用上传下载速率控制功能
#CFLAGS += -D_SPEEDLIMIT_
#定义_SHMMEM_ALLOC_宏则会启用基于共享内存的内存分配器来传输业务数据
CFLAGS += -D_SHMMEM_ALLOC_
#定义_FASTMEM_DEBUG则会打印内存池的统计信息
#CFLAGS += -D_FASTMEM_DEBUG

OBJ	= ../old/tfc_ipc_sv.o \
    ../old/tfc_error.o \
	../base/tfc_base_config_file.o \
	../watchdog/tfc_base_watchdog_client.o	\
	../wtg/wtg_client_api.o	\
	tfc_net_cconn.o \
	tfc_net_ipc_mq.o \
	tfc_net_open_mq.o \
	tfc_net_open_shmalloc.o	\
	tfc_net_raw_cache.o \
	tfc_net_socket_tcp.o \
	tfc_net_ccd_main.o	\
	mydaemon.o			\
	version.o			\
	myalloc.o			\
	fastmem.o			\
	dev_info.o

BIN	= ccd asn_complete.so http_complete.so dumb_complete.so

all: $(BIN)
ccd: $(OBJ)
	$(CXX) $(CFLAGS) -o $@ $^ $(LIB)

#由于64位系统下需要加上-fPIC编译动态库才能通过，所以这里要区分32位和64位下的编译方法
ifeq ($(X86_64),0)
asn_complete.so: asn_complete.o 
	$(CXX) $(CFLAGS) -o $@ $^ -shared
http_complete.so: http_complete.o
	$(CXX) $(CFLAGS) $(INCLUDE) -o $@ $^ -shared -L../libhttp -lhttp -I../libhttp
dumb_complete.so: dumb_complete.o
	$(CXX) $(CFLAGS) -o $@ $^ -shared 
else
asn_complete.so: asn_complete.o 
	$(CXX) $(CFLAGS) -o $@ asn_complete.cpp -fPIC -shared
http_complete.so: http_complete.o
	$(CXX) $(CFLAGS) $(INCLUDE) -o $@ http_complete.cpp -fPIC -shared -L../libhttp -lhttp -I../libhttp
dumb_complete.so: dumb_complete.o
	$(CXX) $(CFLAGS) -o $@ dumb_complete.cpp -fPIC -shared 
endif	

include ../incl_tail.mk
