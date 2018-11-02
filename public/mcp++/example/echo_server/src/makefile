X86_64 = 0
SYSBIT = $(shell getconf LONG_BIT)
ifeq ($(SYSBIT),64)
	X86_64 = 1
endif

ifeq ($(X86_64),0)
	CFLAGS =	-g -O2 -Wall -fno-strict-aliasing -D_GNU_SOURCE
	MCPXX_ = ../../../mcp++_x86
else
	CFLAGS =	-g -O2 -Wall -fno-strict-aliasing -D_GNU_SOURCE -fPIC
	MCPXX_ = ../../../mcp++_x86_64
endif

MCPXX_INC=$(MCPXX_)/inc
MCPXX_BIN=$(MCPXX_)/bin
MCPXX_LIB=$(MCPXX_)/lib
INC =		-I$(MCPXX_INC)/base/. -I$(MCPXX_INC)/old/. -I$(MCPXX_INC)/mcd/. -I$(MCPXX_INC)/ccd/. -I$(MCPXX_INC)/watchdog/.
LIB =	 	-lpthread -L$(MCPXX_LIB)/. -ltfcbase #-lmcptest


ECHO_SERVER = echo_server_mcd.so
COMPLETE    = net_complete.so
BIN =	$(COMPLETE)	\
		$(ECHO_SERVER)
ECHO_SERVER_OBJS =	echo_server.o
COMPLETE_OBJS =	net_complete.o


all: clean $(BIN)

$(ECHO_SERVER): $(ECHO_SERVER_OBJS)
	g++ $(CFLAGS) -o $(ECHO_SERVER) $(ECHO_SERVER_OBJS) $(INC) $(LIB) -shared
$(COMPLETE): $(COMPLETE_OBJS)
	g++ $(CFLAGS) -o $(COMPLETE) $(COMPLETE_OBJS) -shared

%.o: %.cpp
	$(CXX) $(CFLAGS) $(INC) -c -o $@ $<
%.o: %.c
	$(CXX) $(CFLAGS) $(INC) -c -o $@ $<

clean:
	rm -rf $(BIN)
	rm -rf $(ECHO_SERVER_OBJS) $(COMPLETE_OBJS)
	rm -rf *.so *.o

install:
	rm ../bin/{echo_server_ccd,echo_server_mcd,echo_server_watchdog} -f
	cp $(MCPXX_BIN)/{ccd,mcd,watchdog} ../bin/
	cp $(BIN) ../bin/
	ln -sv ccd ../bin/echo_server_ccd
	ln -sv mcd ../bin/echo_server_mcd
	ln -sv watchdog ../bin/echo_server_watchdog

