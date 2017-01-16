AS = wdc816as
CC = wdc816cc
ASFLAGS = -L
CCFLAGS = -LT -ML -I../include

all: instructions.obj hello1.obj hello2.obj  hello3.obj labels.obj ref_only.obj

%.obj : %.c
	$(CC) $(CCFLAGS) $< -o $@


%.obj : %.asm
	$(AS) $(ASFLAGS) $< -o $@
