# BEGIN: You can modify the following region
PROJECT = hello
TOOLCHAIN ?= or1k-elf
DEBUG ?= 0
# TARGET can be either OR1300 (CS-473) or OR1420 (CS-476)
TARGET ?= OR1420
CFLAGS ?=
LDFLAGS ?=
ASFLAGS ?=

CSRCS += $(wildcard src/*.c)
CSRCS += $(wildcard src/coro/*.c)
CSRCS += $(wildcard src/taskman/*.c)
# add other directories here...


SSRCS += $(wildcard src/*.s)
SSRCS += $(wildcard src/coro/*.s)
SSRCS += $(wildcard src/taskman/*.s)
# add other directories here...

# END.

CC = $(TOOLCHAIN)-gcc
LD = $(TOOLCHAIN)-ld
AS = $(TOOLCHAIN)-as
ELF2MEM ?= convert_or32

CSRCS += $(wildcard support/src/*.c)
SSRCS += $(wildcard support/src/*.s)

_LDFLAGS += -nostartfiles
_CFLAGS += -MMD -DPRINTF_INCLUDE_CONFIG_H -I include/ -I support/include

ifeq ($(DEBUG), 1)
BUILD = build-debug
_CFLAGS += -Og -g
else
BUILD = build-release
_CFLAGS += -DNDEBUG -Os
endif

# you can support new targets here...
ifeq ($(TARGET), OR1300)
BUILD := $(BUILD)-or1300
_CFLAGS += -D__OR1300__
_ASFLAGS += --defsym __OR1300__=1
else ifeq ($(TARGET), OR1420)
_CFLAGS += -D__OR1420__ -msoft-div
BUILD := $(BUILD)-or1420
_ASFLAGS += --defsym __OR1420__=1
else
$(error "TARGET variable must be either OR1300 or OR1420!")
endif

OBJS = $(SSRCS:%.s=$(BUILD)/%.s.o) $(CSRCS:%.c=$(BUILD)/%.c.o)
DEPS = $(OBJS:%.o=%.d) # dependencies

ELF = $(addsuffix .elf,$(BUILD)/$(PROJECT))
MEM = $(addsuffix .mem,$(BUILD)/$(PROJECT))

mem : $(MEM)
elf : $(ELF)

$(MEM) : $(ELF)
	mkdir -p $(@D)
	cd $(BUILD); \
		$(ELF2MEM) $(addsuffix .elf,$(PROJECT)); \
		mv $(addsuffix .elf.mem,$(PROJECT)) $(addsuffix .mem,$(PROJECT)); \
		mv $(addsuffix .elf.cmem,$(PROJECT)) $(addsuffix .cmem,$(PROJECT));

# Q: we invoke the linker through $(CC). How to use $(LD) directly?
$(ELF) : $(OBJS)
	mkdir -p $(@D)
	$(CC) -v $(_LDFLAGS) $(LDFLAGS) $^ -o $@

-include $(DEPS)

# user source code
$(BUILD)/src/%.c.o : src/%.c
	mkdir -p $(@D)
	$(CC) $(_CFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/src/%.s.o : src/%.s
	mkdir -p $(@D)
	$(AS) $(_ASFLAGS) $(ASFLAGS) $< -o $@

# for support
$(BUILD)/support/src/%.c.o : support/src/%.c
	mkdir -p $(@D)
	$(CC) $(_CFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/support/src/%.s.o : support/src/%.s
	mkdir -p $(@D)
	$(AS) $(_ASFLAGS) $(ASFLAGS) $< -o $@

.PHONY : clean

clean :
	-rm -rf $(BUILD)/*

# please refer to the followings for more information:
#   https://stackoverflow.com/a/30142139/2604712
#       > Makefile, header dependencies
#   https://www.gnu.org/software/make/manual/html_node/Text-Functions.html
#   https://devhints.io/makefile
#   https://bytes.usc.edu/cs104/wiki/makefile/
#   https://stackoverflow.com/a/3477400/2604712
#       > What do @, - and + do as prefixes to recipe lines in Make?
