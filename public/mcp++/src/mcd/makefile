include ../incl_head.mk

INCLUDE += -I../ccd/ -I../dcc/
CFLAGS += -D_SHMMEM_ALLOC_
# CFLAGS += -D_TDC_DISKCACHE_ -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE
# diskalloc max_size 4K
# CFLAGS += -D_DISKALLOC_LARGE_INDEX_
#TNS_ENABLE: 0 不启用TNS，1 启用TNS
TNS_ENABLE = 0

ifeq ($(TNS_ENABLE),1)
	TNS_DIR = ../tns/
	CFLAGS += -D_ENABLE_TNS_
	INCLUDE += -I$(TNS_DIR)inc/
	LIB += -L$(TNS_DIR)lib/ -lnagent
endif

OBJ	= ../base/tfc_base_config_file.o \
	../old/tfc_ipc_sv.o \
	../ccd/tfc_net_ipc_mq.o \
	../ccd/tfc_net_open_mq.o \
	../ccd/tfc_net_open_shmalloc.o \
	../ccd/mydaemon.o	\
	../watchdog/tfc_base_watchdog_client.o	\
	../wtg/wtg_client_api.o	\
	tfc_cache_chunk_alloc.o \
	tfc_cache_hash_map.o \
	tfc_cache_access.o \
	mydiskalloc.o		\
	tfc_diskcache_hash_map.o \
	tfc_diskcache_access.o \
	tfc_binlog.o \
	tfc_cache_mcd_main.o	\
	tfc_cache_proc.o		\
	version.o				\
	../ccd/myalloc.o

BIN = mcd
# BIN	+= test_app.so http_app.so http_app_fwd.so http_app_cache.so
ifeq ($(TNS_ENABLE),1)
BIN += $(TNS_DIR)lib/libnagent.a $(TNS_DIR)bin/tns_nagent
endif
all: $(BIN)
mcd: $(OBJ)
ifeq ($(TNS_ENABLE),1)
ifeq ($(X86_64),0)
	cp $(TNS_DIR)lib/libnagent_x86.a $(TNS_DIR)lib/libnagent.a
	cp $(TNS_DIR)bin/tns_nagent_x86 $(TNS_DIR)bin/tns_nagent
else
	cp $(TNS_DIR)lib/libnagent_x86_64.a $(TNS_DIR)lib/libnagent.a
	cp $(TNS_DIR)bin/tns_nagent_x86_64 $(TNS_DIR)bin/tns_nagent
endif
	chmod a+x $(TNS_DIR)bin/tns_nagent
endif
	$(CXX) $(CFLAGS) -o $@ $^ $(LIB)

#由于64位系统下需要加上-fPIC编译动态库才能通过，所以这里要区分32位和64位下的编译方法
ifeq ($(X86_64),0)
test_app.so: test_app.o
	$(CXX) $(CFLAGS) $(INCLUDE) -o $@ $^ -shared 
http_app.so: http_app.o ../ccd/tfc_net_ipc_mq.o ../old/tfc_ipc_sv.o ./tfc_cache_proc.o	../ccd/myalloc.o
	$(CXX) $(CFLAGS) $(INCLUDE) -o $@ $^ -shared 
http_app_fwd.so: http_app_fwd.o ../ccd/tfc_net_ipc_mq.o ../old/tfc_ipc_sv.o  ./tfc_cache_proc.o ../ccd/myalloc.o
	$(CXX) $(CFLAGS) $(INCLUDE) -o $@ $^ -shared 
http_app_cache.so: http_app_cache.o ../ccd/tfc_net_ipc_mq.o ../old/tfc_ipc_sv.o ./tfc_cache_proc.o ./tfc_diskcache_access.o ./tfc_diskcache_hash_map.o ./mydiskalloc.o 
	$(CXX) $(CFLAGS) $(INCLUDE) -o $@ $^ -shared 
else
test_app.so: test_app.o
	$(CXX) $(CFLAGS) $(INCLUDE) -o $@ test_app.cpp -fPIC -shared 
http_app.so: http_app.o ../ccd/tfc_net_ipc_mq.o ../old/tfc_ipc_sv.o ./tfc_cache_proc.o	../ccd/myalloc.o
	$(CXX) $(CFLAGS) $(INCLUDE) -o $@ http_app.cpp ../ccd/tfc_net_ipc_mq.cpp ../old/tfc_ipc_sv.cpp tfc_cache_proc.cpp ../ccd/myalloc.cpp -fPIC -shared 
http_app_fwd.so: http_app_fwd.o ../ccd/tfc_net_ipc_mq.o ../old/tfc_ipc_sv.o  ./tfc_cache_proc.o ../ccd/myalloc.o
	$(CXX) $(CFLAGS) $(INCLUDE) -o $@ http_app_fwd.cpp ../ccd/tfc_net_ipc_mq.cpp ../old/tfc_ipc_sv.cpp tfc_cache_proc.cpp ../ccd/myalloc.cpp -fPIC -shared 
http_app_cache.so: http_app_cache.o ../ccd/tfc_net_ipc_mq.o ../old/tfc_ipc_sv.o ./tfc_cache_proc.o ./tfc_diskcache_access.o ./tfc_diskcache_hash_map.o ./mydiskalloc.o 
	$(CXX) $(CFLAGS) $(INCLUDE) -o $@ http_app_cache.cpp ../ccd/tfc_net_ipc_mq.cpp ../old/tfc_ipc_sv.cpp tfc_cache_proc.cpp tfc_diskcache_access.cpp tfc_diskcache_hash_map.cpp mydiskalloc.cpp -fPIC -shared 
endif

include ../incl_tail.mk
