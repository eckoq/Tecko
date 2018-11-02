include ../../incl_head.mk
CC  = gcc
CXX = g++

MAIN_DIR = ../..
CFLAGS   += -g -O2 -Wall -pipe -D_REENTRANT -D_GNU_SOURCE -rdynamic -fPIC -lpthread
INCLUDE  = -I$(MAIN_DIR)/old/. -I$(MAIN_DIR)/base/. -I$(MAIN_DIR)/ccd/ -I$(MAIN_DIR)
TOOL_OBJ = get_mq_stat.o $(MAIN_DIR)/ccd/tfc_net_ipc_mq.o $(MAIN_DIR)/ccd/tfc_net_open_mq.o $(MAIN_DIR)/old/tfc_ipc_sv.o $(MAIN_DIR)/base/tfc_base_config_file.o
LIB      = -lrt

BIN	= get_mq_stat

all: $(BIN)
	strip $(BIN)

get_mq_stat: $(TOOL_OBJ)
	$(CXX) $(CFLAGS) -o $@ $^ $(LIB)

%.o: %.cpp
	$(CXX) $(CFLAGS) $(INCLUDE) -c -o $@ $<
%.o: %.c
	$(CXX) $(CFLAGS) $(INCLUDE) -c -o $@ $<
clean: 
	rm -rf $(OBJ) *.o *.out *~ core* $(BIN)
