CC=gcc

# Should contain pre-built DPDK at least.
RTE_SDK=../deps/dpdk

# Default target, can be overriden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

LDDIRS += -L$(RTE_SDK)/$(RTE_TARGET)/lib	#Here, libdpdk.so should reside.
INC=-I$(RTE_SDK)/$(RTE_TARGET)/include
INC+=-I/usr/include/libxml2	#This is for Ubuntu building. Might be that your libxml2 is somewhere else.

LDLIBS += -ldpdk
LDLIBS += -ldl
LDLIBS += -lxml2
LDLIBS += -lpthread
LDLIBS += -lm

app: sloth_main.o
	$(CC) $(LDDIRS) -o latgen sloth_main.o $(LDLIBS)

sloth_main.o: sloth_main.c sloth_dataplane.c sloth_config.c sloth_main.h
	$(CC) -mssse3 $(INC) -c sloth_main.c

