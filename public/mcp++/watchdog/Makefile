include ../incl_head.mk

CC = gcc
CXX = g++
AR = ar
LFLAGS += -g -Wall -fno-strict-aliasing -fpermissive -D_GNU_SOURCE -D__ENABLE_REPORT -D__OUTPUT_INFO
LDFLAGS += -lpthread -lstdc++
DEPS_INC = -I../common -I../base -I../old -I../
CFLAGS += $(LFLAGS) $(DEPS_INC) -c
DAEMON = watchdog
ODOBJS = watchdog_log.o \
		 watchdog_main.o \
		 watchdog_myhash.o \
		 watchdog_nws_util_str.o \
		 watchdog_qnf_myconfig.o \
		 watchdog_version.o \
		 watchdog_worker.o \
		 log_client.o stat.o tfc_base_config_file.o tfc_md5.o tfc_ipc_sv.o
OLOBJS = watchdog_api.o \
		 watchdog_qnf_myconfig.o \
		 tfc_base_watchdog_client.o
OBJS = $(ODOBJS) $(OLOBJS)
OLIB = libwatchdog.a

all: $(OBJS) $(DAEMON) $(OLIB)

$(DAEMON): $(ODOBJS)
	$(CC) $(LFLAGS) -o $@ $^ $(LDFLAGS)

%.o: ../old/%.cpp
	$(CXX) $(CFLAGS) -I../old -I../base -fPIC -o $@ $<

%.o: ../base/%.cpp
	$(CXX) $(CFLAGS) -I../old -I../base -fPIC -o $@ $<

%.o: ../common/%.cpp
	$(CXX) $(CFLAGS) -I../old -I../base -fPIC -o $@ $<

%.o: %.cc
	$(CXX) $(CFLAGS) $(DEPS_INC) -fPIC -o $@ $<
	
%.o: %.cpp
	$(CXX) $(CFLAGS) -fPIC -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS)  -fPIC -o $@ $<

$(OLIB): $(OLOBJS)
	$(AR) -r $@ $^

clean:
	rm -rf $(DAEMON) $(OLIB) *.o *.gch
