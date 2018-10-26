INCLUDE += -I../old/. -I../base/. -I../ccd/. -I../dcc/. -I../watchdog -I../wtg
LIB     += -ldl -lrt -L../watchdog -lwatchdog -L../wtg -lwtg_api
CFLAGS += -g -O2 -Wall -pipe -D_GNU_SOURCE -rdynamic
#CFLAGS += -D_SHMMEM_ALLOC_
#CFLAGS += -D_SPEEDLIMIT_
OBJ	:= ../base/tfc_base_config_file.o \
	../old/tfc_ipc_sv.o \
	../ccd/tfc_net_ipc_mq.o \
	../ccd/tfc_net_open_mq.o \
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
	version.o

BIN     := mcd test_app.so http_app.so http_app_fwd.so http_app_cache.so
all: $(BIN)
mcd: $(OBJ)
	g++ $(CFLAGS) -o $@ $^ $(LIB)
test_app.so: test_app.o
	g++ $(CFLAGS) $(INCLUDE) -o $@ test_app.cpp -fPIC -shared 
http_app.so: http_app.o ../ccd/tfc_net_ipc_mq.o ../old/tfc_ipc_sv.o ./tfc_cache_proc.o	../ccd/myalloc.o
	g++ $(CFLAGS) $(INCLUDE) -o $@ http_app.cpp ../ccd/tfc_net_ipc_mq.cpp ../old/tfc_ipc_sv.cpp ./tfc_cache_proc.cpp  ../ccd/myalloc.c -fPIC -shared 
http_app_fwd.so: http_app_fwd.o ../ccd/tfc_net_ipc_mq.o ../old/tfc_ipc_sv.o  ./tfc_cache_proc.o ../ccd/myalloc.o
	g++ $(CFLAGS) $(INCLUDE) -o $@ http_app_fwd.cpp ../ccd/tfc_net_ipc_mq.cpp ../old/tfc_ipc_sv.cpp ./tfc_cache_proc.cpp  ../ccd/myalloc.c -fPIC -shared 
http_app_cache.so: http_app_cache.o ../ccd/tfc_net_ipc_mq.o ../old/tfc_ipc_sv.o ./tfc_cache_proc.o ./tfc_diskcache_access.o ./tfc_diskcache_hash_map.o ./mydiskalloc.o 
	g++ $(CFLAGS) $(INCLUDE) -o $@ http_app_cache.cpp ../ccd/tfc_net_ipc_mq.cpp ../old/tfc_ipc_sv.cpp ./tfc_cache_proc.cpp ./tfc_diskcache_access.cpp ./tfc_diskcache_hash_map.cpp ./mydiskalloc.c -fPIC -shared 
%.o: %.cpp
	g++ $(CFLAGS) $(INCLUDE) -c -o $@ $^
%.o: %.c
	g++ $(CFLAGS) $(INCLUDE) -c -o $@ $^
clean: 
	rm -rf $(OBJ) *.o *.out *~ core* $(BIN)
love: clean all
