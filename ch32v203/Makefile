TARGET := rainus
TARGET_MCU := CH32V203C8T6
TARGET_MCU_PACKAGE := CH32V203C8T6
CH32FUN_ROOT ?= ../../ch32fun
HOST_CC ?= cc
BUILD_UNIX_TIME ?= $(shell date -u +%s)
DEBUG_USB ?= 0
ADDITIONAL_C_FILES := sd_spi.c rtc.c event.c chip_id.c log.c fatfs/ff.c
ifeq ($(DEBUG_USB),1)
ADDITIONAL_C_FILES += $(CH32FUN_ROOT)/extralibs/usbd.c
endif
EXTRA_CFLAGS += -Ifatfs -DRAINUS_BUILD_UNIX_TIME=$(BUILD_UNIX_TIME) -DRAINUS_DEBUG_USB=$(DEBUG_USB)

# Keep linker-script generation and all build products in this directory.
override GENERATED_LD_FILE := $(CURDIR)/rainus.generated.ld

.DEFAULT_GOAL := build
include $(CH32FUN_ROOT)/ch32fun/ch32fun.mk

.PHONY: test clean cv_clean
test:
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror event.c tests/event_test.c -o tests/event_test
	./tests/event_test
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror chip_id.c tests/chip_id_test.c -o tests/chip_id_test
	./tests/chip_id_test
	$(HOST_CC) -std=c11 -Wall -Wextra -Ifatfs fatfs/ff.c log.c tests/fatfs_test.c -o tests/fatfs_test
	./tests/fatfs_test

# cv_clean comes from ch32fun and removes the firmware build products.
clean: cv_clean
	$(RM) $(call fixpath,ch32fun.o) $(call fixpath,tests/event_test) $(call fixpath,tests/chip_id_test) $(call fixpath,tests/fatfs_test) $(call fixpath,tests/time_policy_test)
	$(MAKE) -C power_floor clean CH32FUN_ROOT="$(abspath $(CH32FUN_ROOT))"
