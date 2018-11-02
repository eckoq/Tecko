include ../incl_head.mk

CC =		gcc
CXX =		g++
AR =		ar
CFLAGS +=	-g -Wall -O2 -fPIC -pipe -fno-strict-aliasing -D_GNU_SOURCE -D_REENTRANT -c
LDFLAGS +=	-g -Wall -O2 -fPIC

ifeq ($(coverage), yes)
CFLAGS  += -fprofile-arcs -ftest-coverage
LDFLAGS += -lgcov
CFLAGS += -D_OVERLOAD_DEBUG
endif

THIRDPARTY_BIN=../../../thirdparty/bin

all: remove_log_client_shm req_filter_test

remove_log_client_shm : remove_log_client_shm.o
	$(CXX) -o $@ $^ $(LDFLAGS) -L../base -ltfcbase

req_filter_test : req_filter_test.o 
	$(CXX) -o $@ $^ $(LDFLAGS) 

%.o: %.cpp
	$(CXX) $(CFLAGS) -o $@ $< -I../ -I../../

%.o: %.cc
	$(CXX) $(CFLAGS) -o $@ $<  -I../ -I../../

test:
	./req_filter_test

lcov: test
	$(THIRDPARTY_BIN)/lcov -c --list-full-path -d ./ -o req_filter.info
	$(THIRDPARTY_BIN)/genhtml req_filter.info -o req_filter_lcov
	tar czf req_filter_lcov.tar.gz req_filter_lcov

clean:
	rm -rf remove_log_client_shm req_filter_test
	rm -rf *.o *.gcda *.gcno
	rm -rf req_filter.info req_filter_lcov req_filter_lcov.tar.gz
