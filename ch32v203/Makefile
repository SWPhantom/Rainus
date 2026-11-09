TARGET := rainus
TARGET_MCU := CH32V203C8T6
TARGET_MCU_PACKAGE := CH32V203C8T6
CH32FUN_ROOT ?= ../../ch32fun
HOST_CC ?= cc
BUILD_UNIX_TIME ?= $(shell date -u +%s)
ADDITIONAL_C_FILES := sd_spi.c rtc.c event.c time_policy.c fatfs/ff.c $(CH32FUN_ROOT)/extralibs/usbd.c
EXTRA_CFLAGS += -Ifatfs -DRAINUS_BUILD_UNIX_TIME=$(BUILD_UNIX_TIME)

# Keep linker-script generation and all build products in this directory.
override GENERATED_LD_FILE := $(CURDIR)/rainus.generated.ld

.DEFAULT_GOAL := build
include $(CH32FUN_ROOT)/ch32fun/ch32fun.mk

.PHONY: test
test:
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror event.c tests/event_test.c -o tests/event_test
	./tests/event_test
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror time_policy.c tests/time_policy_test.c -o tests/time_policy_test
	./tests/time_policy_test
	$(HOST_CC) -std=c11 -Wall -Wextra -Ifatfs fatfs/ff.c tests/fatfs_test.c -o tests/fatfs_test
	./tests/fatfs_test
