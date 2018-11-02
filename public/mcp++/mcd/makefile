include ../incl_head.mk

INCLUDE += -I../ccd/ -I../dcc/ -I../common/. -I../
CFLAGS += -D_SHMMEM_ALLOC_ #-D_SVRPRX_DEBUG_
# CFLAGS += -D_TDC_DISKCACHE_ -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE
# diskalloc max_size 4K
# CFLAGS += -D_DISKALLOC_LARGE_INDEX_
CFLAGS += -D_OUTPUT_LOG_
LIB += -lz

OBJ = ../ccd/tfc_net_ipc_mq.o \
	../ccd/tfc_net_open_mq.o \
	../ccd/tfc_net_open_shmalloc.o \
	../ccd/mydaemon.o	\
    tfc_cache_chunk_alloc_large.o \
    tfc_cache_hash_map_large.o \
    tfc_cache_access_large.o \
	tfc_cache_chunk_alloc.o \
	tfc_cache_hash_map.o \
	tfc_cache_access.o \
	mydiskalloc.o		\
	tfc_diskcache_hash_map.o \
	tfc_diskcache_access.o \
	tfc_binlog.o \
	tfc_cache_proc.o		\
	../ccd/version.o				\
	../ccd/myalloc.o	\
	../common/log_client.o    \
	../common/stat.o \
	app_engine.o    \
	stat_manager.o    \
    tfc_cache_mcd_main.o \
	app/net_server.o \
	app/net_client.o

BIN = mcd

all: $(BIN)
mcd: $(OBJ)
	$(CXX) $(CFLAGS) -o $@ $^ $(LIB)

include ../incl_tail.mk
