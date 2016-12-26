LINK.o = $(LINK.cc)
CXXFLAGS = -std=c++11 -g
CCFLAGS = -g

OBJS = dumpobj.o disassembler.o

#UNAME_S := $(shell uname -s)
#ifeq ($(UNAME_S),MINGW64_NT-10.0)
ifeq ($(MSYSTEM),MINGW64)
	OBJS += mingw/err.o
	CPPFLAGS += -I mingw/
endif


dumpobj : $(OBJS)


disassembler.o : disassembler.cpp disassembler.h
dumpobj.o : dumpobj.cpp disassembler.h
mingw/err.o : mingw/err.c mingw/err.h

.PHONY: clean
clean:
	$(RM) dumpobj $(OBJS)


.PHONY: variables
variables :
	$(foreach v, $(.VARIABLES), $(info $(v) = $($(v))))
	@echo
