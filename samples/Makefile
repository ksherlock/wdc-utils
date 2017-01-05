AS = wdc816as
ASFLAGS = -L

all: instructions.obj hello1.obj hello2.obj labels.obj

%.obj : %.asm
	$(AS) $(ASFLAGS) $< -o $@
