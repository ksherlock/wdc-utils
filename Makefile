CC=c++ -std=c++11 -g
CXX=c++ -std=c++11 -g
OBJS = dumpobj.o disasm.o

dumpobj : dumpobj.o disassembler.o

disassembler.o : disassembler.cpp disassembler.h
dumpobj.o : dumpobj.cpp disassembler.h

.PHONY:
	clean

clean:
	$(RM) dumpobj $(OBJS)