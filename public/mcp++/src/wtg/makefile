CC =				gcc
CXX =				g++
AR =				ar

CFLAGS =			-g -Wall -fno-strict-aliasing -D_GNU_SOURCE -D__MCP -I../watchdog -c
CFLAGS +=			-D__HARDCODE_CONF
#CFLAGS +=			-D__DEBUG 

TARGETS =			libwtg_daemon.a libwtg_api.a

CPPOBJ =			wtg_client_api.o

M_SLIB =			wtg_master_plugin.so

DAEMON_OBJS =		daemon_module.o	\
					wtg_version.o	\
					wtg_hash.o

API_OBJS =			wtg_api.o

all: $(TARGETS) $(M_SLIB) $(CPPOBJ)

$(TARGETS): $(DAEMON_OBJS) $(API_OBJS)
	$(AR) -r libwtg_daemon.a $(DAEMON_OBJS)
	$(AR) -r libwtg_api.a $(API_OBJS)

%.o: %.cpp
	$(CXX) $(CFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

$(M_SLIB):
	$(CXX) -g -Wall -fPIC -shared -o $(M_SLIB) wtg_master_plugin.cpp

clean:
	rm -rf $(TARGETS) $(M_SLIB) $(DAEMON_OBJS) $(API_OBJS) *.o *.gch
