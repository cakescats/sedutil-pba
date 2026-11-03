# Host tests, no drive needed: make -C tests/host check
TOP := ../..
CXX ?= c++
CC ?= cc
CPPFLAGS := -I$(TOP)/Common -I$(TOP)/Common/pbkdf2 -I$(TOP)/linux -I$(TOP)/LinuxPBA -I.
CXXFLAGS := -std=c++11 -Wall -Werror -O2
CFLAGS := -std=c11 -Wall -Werror -O2
PBKDF2 := blockwise.c chash.c hmac.c pbkdf2.c sha1.c sha512.c

OBJS := test_main.o DtaHashPwd.o PbaConfig.o PbaSso.o $(PBKDF2:.c=.o)

vpath %.cpp $(TOP)/Common $(TOP)/LinuxPBA
vpath %.c $(TOP)/Common/pbkdf2

test_host: Version.h $(OBJS)
	$(CXX) -o $@ $(OBJS)

Version.h:
	echo '#define GIT_VERSION "host-test"' > $@

$(OBJS): Version.h

check: test_host
	./test_host $$(python3 vectors.py) | tee test_host.log
	grep -q 'validation suite ... passed' test_host.log

clean:
	rm -f test_host test_host.log *.o Version.h

.PHONY: check clean
