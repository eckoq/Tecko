include ../../incl_head.mk

CXX=g++
CFLAGS+=-g -D_GNU_SOURCE -Wall

INC=-I../../base -I../../old -I../../common -I../../
LIB=#../lib/libnagent.a -lz

TAEGET=mcp_trace

OBJ=../../common/log_client.o ../../base/tfc_base_config_file.o ../../base/tfc_md5.o ../../old/tfc_ipc_sv.o ../../common/stat.o

all: $(TAEGET)
$(TAEGET):$(OBJ) mcp_trace_main.o 
	$(CXX) $(CFLAGS) -o $@ $^ $(LIB) 

%.o:%.cpp
	$(CXX) $(CFLAGS) $(INC) -c $< -o $@

clean:
	rm -f *.o 
	rm -f $(TAEGET)
