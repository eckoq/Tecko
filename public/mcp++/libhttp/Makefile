include ../incl_head.mk

CC =				gcc
CXX =				g++
AR =				ar
LIB =				http
LFLAGS +=			-g -Wall -fno-strict-aliasing -I../base -D_GNU_SOURCE
DFLAGS =			$(LFLAGS) -fPIC -shared
CFLAGS =			$(LFLAGS) -fPIC -c
SLOBJS =			http_api.o
CXXLIBOBJS =		http_api.o tfc_base_http.o
DLOBJS =			http_complete.o tfc_base_http.o
OBJS =				http_api.o http_complete.o tfc_base_http.o
SLIB =				libhttp.a
CXXLIB =			libhttpcxx.a
DLIB =				http_complete.so

all: $(OBJS) $(SLIB) $(CXXLIB) $(DLIB)

$(SLIB): $(SLOBJS)
	$(AR) -r $@ $^

$(CXXLIB): $(CXXLIBOBJS)
	$(AR) -r $@ $^

$(DLIB): $(DLOBJS) $(SLIB)
	$(CXX) $(DFLAGS) -o $(DLIB) http_complete.cpp -L./ -lhttp

%.o: %.cpp
	$(CXX) $(CFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rf $(OBJS) $(SLIB) $(CXXLIB) $(DLIB) $(TEST) $(UTILITY) $(TEST_MCD_SERVER) *.o *.gch

