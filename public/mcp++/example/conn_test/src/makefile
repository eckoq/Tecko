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
INC =		-I$(MCPXX_INC)/base/. -I$(MCPXX_INC)/old/. -I$(MCPXX_INC)/ccd/. -I$(MCPXX_INC)/mcd/. -I$(MCPXX_INC)/dcc/. -I$(MCPXX_INC)/watchdog/.
LIB =	 	-lpthread -L$(MCPXX_LIB)/. -ltfcbase #-lmcptest


CONN_TEST =	conn_test_mcd.so
COMPLETE =	net_complete.so
BIN =	$(CONN_TEST)	\
	$(COMPLETE)
CONN_TEST_OBJS =	conn_test.o
COMPLETE_OBJS = net_complete.o


all: clean $(BIN)

$(CONN_TEST): $(CONN_TEST_OBJS)
	g++ $(CFLAGS) -o $(CONN_TEST) $(CONN_TEST_OBJS) $(INC) $(LIB) -shared
$(COMPLETE): $(COMPLETE_OBJS)
	g++ $(CFLAGS) -o $(COMPLETE) $(COMPLETE_OBJS) -shared

%.o: %.cpp
	$(CXX) $(CFLAGS) $(INC) -c -o $@ $<
%.o: %.c
	$(CXX) $(CFLAGS) $(INC) -c -o $@ $<

clean:
	rm -rf $(BIN)
	rm -rf $(CONN_TEST_OBJS) $(COMPLETE_OBJS)
	rm -rf *.so *.o

install:
	rm ../bin/conn_test_{mcd,dcc,watchdog} -f
	cp $(MCPXX_BIN)/{mcd,dcc,watchdog} ../bin/
	cp $(BIN) ../bin/
	ln -sv mcd ../bin/conn_test_mcd
	ln -sv dcc ../bin/conn_test_dcc
	ln -sv watchdog ../bin/conn_test_watchdog
