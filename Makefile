LINK.o = $(LINK.cc)
CXXFLAGS = -std=c++11 -g
CCFLAGS = -g
CPPFLAGS = -I .

UNAME_S := $(shell uname -s)
OBJS = dumpobj.o disassembler.o

#ifeq ($(UNAME_S),MINGW64_NT-10.0)
ifeq ($(MSYSTEM),MINGW64)
	OBJS += err.o
endif

.PHONY: variables
variables :
	$(foreach v, $(.VARIABLES), $(info $(v) = $($(v))))
	@echo

dumpobj : $(OBJS)


disassembler.o : disassembler.cpp disassembler.h
dumpobj.o : dumpobj.cpp disassembler.h
err.o : err.c err.h

.PHONY: clean
clean:
	$(RM) dumpobj $(OBJS)