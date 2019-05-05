DEBUG = FALSE

SOURCES = \
	src/common/buffer.c \
	src/common/err.c \
	src/common/fastmem.c \
	src/common/files.c \
	src/common/hotkey.c \
	src/common/input.c \
	src/common/main.c \
	src/common/math.c \
	src/common/object.c \
	src/common/process.c \
	src/common/quetzal.c \
	src/common/random.c \
	src/common/redirect.c \
	src/common/screen.c \
	src/common/sound.c \
	src/common/stream.c \
	src/common/table.c \
	src/common/text.c \
	src/common/variable.c \
	src/dumb/dumb_init.c \
	src/dumb/dumb_input.c \
	src/dumb/dumb_output.c \
	src/dumb/dumb_pic.c
EXE = nFrotz

OBJECTS = $(SOURCES:.c=.o)

CC = nspire-gcc
LD = nspire-ld
GENZEHN = genzehn

CFLAGS = -Wall -Wextra -marm -I../../include -Os -DVERSION="\"2.43d\""
LDFLAGS = ../../lib/libnspireio.a
LIBS = -lnspireio
ZEHNFLAGS = --name "nFrotz"

ifeq ($(DEBUG),FALSE)
        CFLAGS += -Os
else
        CFLAGS += -O0 -g
endif


DISTDIR = ./bin
vpath %.tns $(DISTDIR)
vpath %.elf $(DISTDIR)


all: $(EXE).prg.tns

$(EXE).elf: $(OBJECTS)
		$(LD) $(LDFLAGS) $^ $(LIBS) -o $(@:.tns=.elf)

$(EXE).tns: $(EXE).elf
		$(GENZEHN) --input $^ --output $@ $(ZEHNFLAGS)

$(EXE).prg.tns: $(EXE).tns
		make-prg $^ $@

.c.o:
		$(CC) $(CFLAGS) -c $< -o $@

clean:
		rm -f $(OBJECTS) $(DISTDIR)/$(EXE).tns $(DISTDIR)/$(EXE).elf $(DISTDIR)/$(EXE).prg.tns
