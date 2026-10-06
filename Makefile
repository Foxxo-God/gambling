GBA_CC ?= clang
GBA_LD ?= $(shell command -v ld.lld-18 2>/dev/null || command -v ld.lld 2>/dev/null || echo ld.lld)
TARGET_FLAGS = --target=arm-none-eabi -mcpu=arm7tdmi -marm
CFLAGS = $(TARGET_FLAGS) -O2 -ffreestanding -fno-builtin -fno-unwind-tables
BUILD_DIR = build
ROM = lucky-circuit.gba

.PHONY: all clean

all: $(ROM)

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/main.o: gba/main.c | $(BUILD_DIR)
	$(GBA_CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/startup.o: gba/startup.s | $(BUILD_DIR)
	$(GBA_CC) $(TARGET_FLAGS) -c $< -o $@

$(ROM): $(BUILD_DIR)/main.o $(BUILD_DIR)/startup.o gba/linker.ld
	$(GBA_LD) -T gba/linker.ld --gc-sections --oformat=binary $(BUILD_DIR)/startup.o $(BUILD_DIR)/main.o -o $@

clean:
	rm -rf $(BUILD_DIR) $(ROM)