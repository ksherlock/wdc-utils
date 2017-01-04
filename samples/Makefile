AS = wdc816as
ASFLAGS = -L

all: instructions.obj

%.obj : %.asm
	$(AS) $(ASFLAGS) $< -o $@
