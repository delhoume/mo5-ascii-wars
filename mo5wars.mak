ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

CMOC ?= cmoc
CMOC_FLAGS ?= --thommo --org=2600 -Wno-assign-in-condition -D__CMOC__ -I$(ROOT)/src

BUILD_DIR := $(ROOT)/build
DISK_STAGE := $(BUILD_DIR)/disk
OUTPUT_DIR := $(ROOT)/output
PROGRAM_BIN := $(BUILD_DIR)/MO5WARS.BIN
DISK_IMAGE := $(OUTPUT_DIR)/mo5wars.fd

SOURCES := \
	$(ROOT)/src/main.c \
	$(ROOT)/src/reader.c \
	$(ROOT)/src/memoryreader.c \
	$(ROOT)/src/filereader.c \
	$(ROOT)/src/ascii_decoder.c

HEADERS := \
	$(ROOT)/src/ascii_decoder.h \
	$(ROOT)/src/reader.h \
	$(ROOT)/src/memoryreader.h \
	$(ROOT)/src/filereader.h \
	$(ROOT)/src/delays.h \
	$(ROOT)/src/nitram5x5_mini.h \
	$(ROOT)/src/short7.h

FDFS := $(ROOT)/dist/fdfs
BOOTLOADER := $(ROOT)/dist/BOOTMO.BIN
ANIMATION := $(ROOT)/dist/WAR300_7.RAW
SHORT_ANIMATION := $(ROOT)/dist/SHORT7.RAW
ANIMATION8 := $(ROOT)/dist/WAR300_8.RAW
SHORT_ANIMATION8 := $(ROOT)/dist/SHORT8.RAW

.PHONY: all binary disk

all: disk

binary: $(PROGRAM_BIN)

disk: $(DISK_IMAGE)

$(PROGRAM_BIN): $(SOURCES) $(HEADERS)
	mkdir -p "$(BUILD_DIR)"
	$(CMOC) $(CMOC_FLAGS) -o "$@" $(SOURCES)

$(DISK_IMAGE): $(PROGRAM_BIN) $(FDFS) $(BOOTLOADER) $(ANIMATION) $(SHORT_ANIMATION) $(ANIMATION8) $(SHORT_ANIMATION8)
	mkdir -p "$(DISK_STAGE)" "$(OUTPUT_DIR)"
	cp "$(PROGRAM_BIN)" "$(DISK_STAGE)/MO5WARS.BIN"
	cp "$(BOOTLOADER)" "$(DISK_STAGE)/BOOTMO.BIN"
	cp "$(ANIMATION)" "$(DISK_STAGE)/WARS7.RAW"
	cp "$(SHORT_ANIMATION)" "$(DISK_STAGE)/SHORT7.RAW"
	cp "$(ANIMATION8)" "$(DISK_STAGE)/WARS8.RAW"
	cp "$(SHORT_ANIMATION8)" "$(DISK_STAGE)/SHORT8.RAW"
	cd "$(DISK_STAGE)" && "$(FDFS)" -addBL "$(DISK_IMAGE)" \
		BOOTMO.BIN MO5WARS.BIN WARS7.RAW SHORT7.RAW WARS8.RAW SHORT8.RAW
