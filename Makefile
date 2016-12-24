CC=c++ -std=c++11 -g
CXX=c++ -std=c++11 -g
OBJS = dumpobj.o disasm.o

dumpobj : dumpobj.o disasm.o

.PHONY:
	clean

clean:
	$(RM) dumpobj $(OBJS)