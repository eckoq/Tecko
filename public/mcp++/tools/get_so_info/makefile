include ../../incl_head.mk
CC = gcc
CXX = g++

MAIN_DIR= ../..
INCLUDE = -I$(MAIN_DIR)/old/. -I$(MAIN_DIR)/base/. -I$(MAIN_DIR)/watchdog -I$(MAIN_DIR)/ccd/ -I$(MAIN_DIR)/dcc/ -I$(MAIN_DIR)/mcd/ -I$(MAIN_DIR)/mcd/app -I$(MAIN_DIR)/common/. -I$(MAIN_DIR)
LIB     = -ldl -lpthread -lrt -lz -L$(MAIN_DIR)/base -ltfcbase -L$(MAIN_DIR)/watchdog -lwatchdog
CFLAGS  += -g -O2 -Wall -fPIC -pipe -D_REENTRANT -D_GNU_SOURCE -rdynamic -D_CACHE_COMPLETE_CHECK -D_SHMMEM_ALLOC_


OBJ= $(MAIN_DIR)/ccd/tfc_net_ipc_mq.o         \
	 $(MAIN_DIR)/ccd/tfc_net_open_mq.o        \
	 $(MAIN_DIR)/ccd/tfc_net_open_shmalloc.o  \
	 $(MAIN_DIR)/ccd/mydaemon.o	              \
	 $(MAIN_DIR)/ccd/version.o				  \
	 $(MAIN_DIR)/ccd/myalloc.o	              \
	 $(MAIN_DIR)/mcd/tfc_cache_chunk_alloc.o  \
	 $(MAIN_DIR)/mcd/tfc_cache_hash_map.o     \
	 $(MAIN_DIR)/mcd/tfc_cache_access.o       \
	 $(MAIN_DIR)/mcd/mydiskalloc.o		      \
	 $(MAIN_DIR)/mcd/tfc_diskcache_hash_map.o \
	 $(MAIN_DIR)/mcd/tfc_diskcache_access.o   \
	 $(MAIN_DIR)/mcd/tfc_binlog.o             \
	 $(MAIN_DIR)/mcd/tfc_cache_proc.o		  \
	 $(MAIN_DIR)/mcd/mcppp_reportor.o         \
	 $(MAIN_DIR)/mcd/app_engine.o             \
	 $(MAIN_DIR)/mcd/app/net_server.o		  \
	 $(MAIN_DIR)/mcd/app/net_client.o 		  \
	 $(MAIN_DIR)/mcd/stat_manager.o           \
	 $(MAIN_DIR)/ccd/version.o                \
     $(MAIN_DIR)/common/log_client.o          \
     get_so_info.o

BIN = get_so_info

all: $(BIN)
	strip $(BIN)

get_so_info: $(OBJ)
	$(CXX) $(CFLAGS) -o $@ $^ $(LIB)

%.o: %.cpp
	$(CXX) $(CFLAGS) $(INCLUDE) -c -o $@ $<
%.o: %.c
	$(CXX) $(CFLAGS) $(INCLUDE) -c -o $@ $<
clean: 
	rm -rf $(OBJ) *.o *.out *~ core* $(BIN)
