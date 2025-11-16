# Makefile for Samsung_SnapKiller
# A tool to kill Samsung's snap-service

CC := aarch64-linux-gnu-gcc
CFLAGS := -O2 -static -s -Wall -Wextra
TARGET := monitor_snap
SRC := monitor_snap.c

# Module configuration
MODULE_NAME := SamsungSnapKiller
MODULE_ID := SnapKiller
MODULE_VERSION := v1.0
MODULE_VERSION_CODE := 10
MODULE_AUTHOR := Flopster101

# Build configuration
OUT_DIR := out
MODULE_DIR := $(OUT_DIR)/module
GIT_HASH := $(shell git rev-parse --short HEAD 2>/dev/null || echo "nogit")
MODULE_ZIP := $(OUT_DIR)/$(MODULE_ID)-$(MODULE_VERSION)-$(GIT_HASH).zip

.PHONY: all binary module clean install native

all: binary

binary: $(TARGET)

$(TARGET): $(SRC)
	@mkdir -p $(OUT_DIR)
	$(CC) $(CFLAGS) -o $(OUT_DIR)/$(TARGET) $(SRC)

module: binary
	@echo "Building Magisk/KSU module..."
	@rm -rf $(MODULE_DIR)
	@mkdir -p $(MODULE_DIR)
	@cp $(OUT_DIR)/$(TARGET) $(MODULE_DIR)/
	@./build_module.sh "$(MODULE_DIR)" "$(MODULE_ID)" "$(MODULE_NAME)" "$(MODULE_VERSION)-$(GIT_HASH)" "$(MODULE_VERSION_CODE)" "$(MODULE_AUTHOR)"
	@cd $(MODULE_DIR) && zip -r9 ../$(MODULE_ID)-$(MODULE_VERSION)-$(GIT_HASH).zip . -x "*.git*"
	@echo "Module created: $(MODULE_ZIP)"

clean:
	rm -rf $(OUT_DIR)

install: $(TARGET)
	install -m 755 $(TARGET) /system/bin/$(TARGET)

# For native builds (if building on the device itself)
native:
	$(MAKE) CC=gcc
