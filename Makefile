LINK.o = $(LINK.cc)
CXXFLAGS = -std=c++14 -g -Wall
CCFLAGS = -g

DUMP_OBJS = dumpobj.o disassembler.o zrdz_disassembler.o
LINK_OBJS = link.o expression.o

#UNAME_S := $(shell uname -s)
#ifeq ($(UNAME_S),MINGW64_NT-10.0)
ifeq ($(MSYSTEM),MINGW64)
	DUMP_OBJS += mingw/err.o
	LINK_OBJS += mingw/err.o
	CPPFLAGS += -I mingw/
	LDLIBS += -static
endif

.PHONY: all
all: wdcdumpobj wdclink

wdcdumpobj : $(DUMP_OBJS)
	$(LINK.o) $^ $(LDLIBS) -o $@

wdclink : $(LINK_OBJS)
	$(LINK.o) $^ $(LDLIBS) -o $@


disassembler.o : disassembler.cpp disassembler.h
zrdz_disassembler.o : zrdz_disassembler.cpp zrdz_disassembler.h disassembler.h
dumpobj.o : dumpobj.cpp zrdz_disassembler.h disassembler.h
mingw/err.o : mingw/err.c mingw/err.h

.PHONY: clean
clean:
	$(RM) wdcdumpobj $(DUMP_OBJS) $(LINK_OBJS)


.PHONY: variables
variables :
	$(foreach v, $(.VARIABLES), $(info $(v) = $($(v))))
	@echo
