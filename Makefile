# Casio Loopy cartridge. Run make in this directory.
.DEFAULT_GOAL := engine

CROSS ?= sh-elf-
CC := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy
PYTHON ?= python3
BUILD ?= build
OPT ?= -O2
STACKSIZE ?= 8K
IWAD ?= doom1.wad

ENGINE := $(BUILD)/doom-loopy-engine.bin
ROM := $(BUILD)/doom-loopy.bin
ELF := $(BUILD)/doom-loopy.elf
LDSCRIPT := loopy/tools/loopy.ld

LINUX_SOURCES := d_net.c f_wipe.c i_main.c i_net.c i_sound.c i_system.c \
                 i_video.c m_swap.c p_saveg.c s_sound.c w_wad.c
DOOM_SOURCES := $(filter-out $(addprefix doom/,$(LINUX_SOURCES)),$(wildcard doom/*.c))
C_SOURCES := $(DOOM_SOURCES) $(wildcard loopy/src/*.c platform/*.c runtime/*.c wad/*.c)
S_SOURCES := $(wildcard loopy/src/*.s platform/*.S)
OBJECTS := $(addprefix $(BUILD)/,$(C_SOURCES:.c=.o) $(S_SOURCES:.s=.o))
OBJECTS := $(OBJECTS:.S=.o)

TARGET := -m1 -mb -mrenesas
CPPFLAGS += -Idoom -Iloopy/include
CFLAGS += $(TARGET) $(OPT) -g -std=gnu99 -ffreestanding -fcommon \
          -fwrapv -fno-strict-aliasing -falign-functions=4 \
          -ffunction-sections -fdata-sections -fno-unwind-tables \
          -fno-asynchronous-unwind-tables -fomit-frame-pointer -MMD -MP
LDFLAGS += $(TARGET) -nostdlib -Wl,--gc-sections -Wl,--no-warn-rwx-segment \
           -Wl,--print-memory-usage -Wl,-Map,$(BUILD)/doom-loopy.map \
           -Wl,--defsym=SRAMSIZE=8K -Wl,--defsym=STACKSIZE=$(STACKSIZE) \
           -Wl,-T,$(LDSCRIPT)

.PHONY: engine rom clean
engine: $(ENGINE)

rom: $(ENGINE)
	$(PYTHON) wad/build_rom.py "$(ENGINE)" "$(IWAD)" "$(ROM)"

$(ELF): $(OBJECTS) Makefile $(wildcard loopy/tools/*.ld)
	$(CC) $(LDFLAGS) $(OBJECTS) -Wl,--start-group -lc -lgcc -Wl,--end-group -o $@

$(ENGINE): $(ELF) loopy/tools/fixrom.py
	$(OBJCOPY) -O binary $< $@
	$(PYTHON) loopy/tools/fixrom.py $@

$(BUILD)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.s
	@mkdir -p $(@D)
	$(CC) $(TARGET) -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(TARGET) -c $< -o $@

clean:
	rm -rf "$(BUILD)"

-include $(OBJECTS:.o=.d)
