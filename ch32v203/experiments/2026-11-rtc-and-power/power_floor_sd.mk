TARGET := power_floor
TARGET_MCU := CH32V203C8T6
TARGET_MCU_PACKAGE := CH32V203C8T6
CH32FUN_ROOT ?= ../../../ch32fun
ADDITIONAL_C_FILES := ../sd_spi.c
EXTRA_CFLAGS += -I../fatfs

override GENERATED_LD_FILE := $(CURDIR)/power_floor.generated.ld
.DEFAULT_GOAL := build
include $(CH32FUN_ROOT)/ch32fun/ch32fun.mk

.PHONY: clean cv_clean
clean: cv_clean
