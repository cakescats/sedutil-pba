# Host tests, no drive needed: make -C tests/host check
TOP := ../..
CXX ?= c++
CC ?= cc
CPPFLAGS := -I$(TOP)/Common -I$(TOP)/Common/pbkdf2 -I$(TOP)/linux -I$(TOP)/LinuxPBA -I.
CXXFLAGS := -std=c++11 -Wall -Werror -O2
CFLAGS := -std=c11 -Wall -Werror -O2
PBKDF2 := blockwise.c chash.c hmac.c pbkdf2.c sha1.c sha512.c

OBJS := test_main.o DtaHashPwd.o PbaConfig.o PbaSso.o $(PBKDF2:.c=.o)

# the whole sedutil-cli on a stand-in drive (FakeDevOS.cpp) for cli_test.py
CLI_SRC := sedutil.cpp DtaOptions.cpp DtaCli.cpp DtaJson.cpp DtaAnnotatedDump.cpp \
	DtaCommand.cpp DtaDev.cpp DtaDevEnterprise.cpp DtaDevGeneric.cpp DtaDevOpal.cpp \
	DtaDevOpal1.cpp DtaDevOpal2.cpp DtaDiskType.cpp DtaHexDump.cpp DtaResponse.cpp \
	DtaSession.cpp FakeDevOS.cpp
CLI_OBJS := $(CLI_SRC:.cpp=.o) DtaHashPwd.o $(PBKDF2:.c=.o)

vpath %.cpp $(TOP)/Common $(TOP)/LinuxPBA
vpath %.c $(TOP)/Common/pbkdf2

test_host: Version.h $(OBJS)
	$(CXX) -o $@ $(OBJS)

Version.h:
	echo '#define GIT_VERSION "host-test"' > $@

sedutil-cli-fake: Version.h $(CLI_OBJS)
	$(CXX) -o $@ $(CLI_OBJS)

$(OBJS) $(CLI_OBJS): Version.h

check: test_host sedutil-cli-fake
	./test_host $$(python3 vectors.py) | tee test_host.log
	grep -q 'validation suite ... passed' test_host.log
	python3 cli_test.py ./sedutil-cli-fake

clean:
	rm -f test_host test_host.log sedutil-cli-fake *.o Version.h

.PHONY: check clean
