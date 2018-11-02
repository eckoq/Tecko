include ../incl_head.mk

CC =		gcc
CXX =		g++
AR =		ar
CFLAGS +=	-g -Wall -O2 -fPIC -pipe -fno-strict-aliasing -fpermissive -D_GNU_SOURCE -D_REENTRANT -c
LDFLAGS +=	-g -Wall -O2 -fPIC
OLOBJS =	tfc_base_config_file.o	\
			tfc_base_fast_timer.o	\
			tfc_base_log.o	\
			tfc_base_parsepara.o	\
			tfc_base_timer.o \
			tfc_md5.o \
			ftlib.o \
			asn13_complete.o \
			asn20_complete.o \
			../old/tfc_debug_log.o \
			../old/tfc_error.o \
			../old/tfc_ipc_sv.o \
			../old/tfc_thread_sync.o \
			../old/tfc_stat.o \
			../old/tfc_stat_v2.o
OLIB =		libtfcbase.a
BIN = asn13_complete.so asn20_complete.so

all: clean target
	make -f mkfile.real

target: $(OLOBJS) $(OLIB) $(BIN)

%.o: %.cpp
	$(CXX) $(CFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

asn13_complete.so: asn13_complete_so.cpp asn13_complete.cpp
	$(CXX) $(LDFLAGS) -o $@ $^ -shared
asn20_complete.so: asn20_complete_so.cpp asn20_complete.cpp
	$(CXX) $(LDFLAGS) -o $@ $^ -shared

$(OLIB): $(OLOBJS)
	$(AR) -r $@ $^

clean:
	rm -rf $(OLIB) $(OLOBJS) $(BIN) *.gch
