LINK.o = $(LINK.cc)
CXXFLAGS = -std=c++11 -g -Wall
CCFLAGS = -g

OBJS = dumpobj.o disassembler.o zrdz_disassembler.o

#UNAME_S := $(shell uname -s)
#ifeq ($(UNAME_S),MINGW64_NT-10.0)
ifeq ($(MSYSTEM),MINGW64)
	OBJS += mingw/err.o
	CPPFLAGS += -I mingw/
	LDLIBS += -static
endif


wdcdumpobj : $(OBJS)
	$(LINK.o) $^ $(LDLIBS) -o $@

disassembler.o : disassembler.cpp disassembler.h
zrdz_disassembler.o : zrdz_disassembler.cpp zrdz_disassembler.h disassembler.h
dumpobj.o : dumpobj.cpp zrdz_disassembler.h disassembler.h
mingw/err.o : mingw/err.c mingw/err.h

.PHONY: clean
clean:
	$(RM) wdcdumpobj $(OBJS)


.PHONY: variables
variables :
	$(foreach v, $(.VARIABLES), $(info $(v) = $($(v))))
	@echo
