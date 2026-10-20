EE_BIN = ashfog.elf
EE_OBJS = src/main_ps2.o src/game.o src/render.o
EE_INCS += -Isrc
EE_CFLAGS += -O2 -Wall
EE_LIBS = -lgskit -ldmakit -lpad -lc -lm -lkernel
all: $(EE_BIN)
clean:
	rm -f $(EE_BIN) $(EE_OBJS)
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
