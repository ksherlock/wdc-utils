LINK.o = $(LINK.cc)
CXXFLAGS = -std=c++14 -g -Wall -Wno-sign-compare 
CCFLAGS = -g

DUMP_OBJS = dumpobj.o disassembler.o zrdz_disassembler.o
LINK_OBJS = link.o expression.o omf.o set_file_type.o afp/libafp.a

# static link if using mingw32 or mingw64 to make redistribution easier.
# also add mingw directory.
ifeq ($(MSYSTEM),MINGW32)
	DUMP_OBJS += mingw/err.o
	LINK_OBJS += mingw/err.o
	CPPFLAGS += -I mingw/
	LDLIBS += -static
endif

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


subdirs :
	$(MAKE) -C afp

disassembler.o : disassembler.cpp disassembler.h
zrdz_disassembler.o : zrdz_disassembler.cpp zrdz_disassembler.h disassembler.h
dumpobj.o : dumpobj.cpp zrdz_disassembler.h disassembler.h
omf.o : omf.cpp omf.h
expression.o : expression.cpp expression.h
mingw/err.o : mingw/err.c mingw/err.h

set_file_type.o : CPPFLAGS += -I afp/include
set_file_type.o : set_file_type.cpp

afp/libafp.a : subdirs

.PHONY: clean
clean:
	$(RM) wdcdumpobj wdclink $(DUMP_OBJS) $(LINK_OBJS)
	$(MAKE) -C afp clean


.PHONY: variables
variables :
	$(foreach v, $(.VARIABLES), $(info $(v) = $($(v))))
	@echo
