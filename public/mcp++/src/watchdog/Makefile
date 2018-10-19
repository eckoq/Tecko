CC =				gcc
CXX =				g++ 
AR =				ar
LFLAGS =			-g -Wall -fno-strict-aliasing -I../wtg -D_GNU_SOURCE -D__ENABLE_REPORT -D__OUTPUT_INFO
CFLAGS =			$(LFLAGS) -c
DAEMON =			watchdog
ODOBJS =			watchdog_log.o	\
				watchdog_main.o	\
				watchdog_myhash.o	\
				watchdog_nws_util_str.o	\
				watchdog_qnf_myconfig.o	\
				watchdog_version.o	\
				watchdog_worker.o
OLOBJS =			watchdog_api.o watchdog_qnf_myconfig.o
OBJS =				$(ODOBJS) $(OLOBJS)	\
				tfc_base_watchdog_client.o	\
				watchdog_api.o
OLIB =				libwatchdog.a

all: $(OBJS) $(DAEMON) $(OLIB)

$(DAEMON): $(ODOBJS)
	$(CC) $(LFLAGS) -o $@ $^ -L../wtg -lwtg_daemon -lwtg_api -lpthread

%.o: %.cpp
	$(CXX) $(CFLAGS) -fPIC -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -fPIC -o $@ $<

$(OLIB): $(OLOBJS)
	$(AR) -r $@ $^

clean:
	rm -rf $(DAEMON) $(OLIB) *.o *.gch

