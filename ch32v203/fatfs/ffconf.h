/* Rainus configuration for the bundled FatFs R0.15 filesystem library.
 * Macro names are required by FatFs; 0 generally disables a feature.
 * We support FAT16/FAT32, one drive, 512-byte sectors, short filenames, and
 * synchronous access from the main loop only. No on-device formatting.
 * The library sources ff.c/ff.h/diskio.h are kept as supplied upstream.
 */
#define FFCONF_DEF 80286
#define FF_FS_READONLY 0
#define FF_FS_MINIMIZE 0
#define FF_USE_FIND 0
#define FF_USE_MKFS 0 /* No card-formatting API in the firmware. */
#define FF_USE_FASTSEEK 0 /* Retain the open file's cursor instead of a seek map. */
#define FF_USE_EXPAND 0
#define FF_USE_CHMOD 0
#define FF_USE_LABEL 0
#define FF_USE_FORWARD 0
#define FF_USE_STRFUNC 0
#define FF_PRINT_LLI 0
#define FF_PRINT_FLOAT 0
#define FF_STRF_ENCODE 3
#define FF_CODE_PAGE 437
#define FF_USE_LFN 0 /* No long filenames; RAINLOG.TXT fits the 8.3 format. */
#define FF_MAX_LFN 255
#define FF_LFN_UNICODE 0
#define FF_LFN_BUF 255
#define FF_SFN_BUF 12
#define FF_FS_RPATH 0
#define FF_VOLUMES 1
#define FF_STR_VOLUME_ID 0
#define FF_VOLUME_STRS "SD"
#define FF_MULTI_PARTITION 0
#define FF_MIN_SS 512
#define FF_MAX_SS 512
#define FF_LBA64 0
#define FF_MIN_GPT 0x10000000
#define FF_USE_TRIM 0
#define FF_FS_TINY 1 /* Share one filesystem sector buffer to save RAM. */
#define FF_FS_EXFAT 0 /* exFAT cards must be reformatted to FAT32 on a computer. */
#define FF_FS_NORTC 1 /* Fixed file metadata dates; CSV uses our real RTC times. */
#define FF_NORTC_MON 1
#define FF_NORTC_MDAY 1
#define FF_NORTC_YEAR 2026
#define FF_FS_NOFSINFO 0
#define FF_FS_LOCK 0 /* No concurrent file handles to protect. */
#define FF_FS_REENTRANT 0 /* Interrupt handlers never call the filesystem. */
#define FF_FS_TIMEOUT 1000
