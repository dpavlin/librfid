#ifndef _RFID_H
#define _RFID_H

#include <stdio.h>

#ifdef __LIBRFID__

#include <librfid/rfid_system.h>

enum rfid_frametype {
	RFID_14443A_FRAME_REGULAR,
	RFID_14443B_FRAME_REGULAR,
	RFID_MIFARE_FRAME,
};

#define DEBUG_LIBRFID

#ifdef DEBUG_LIBRFID

#ifdef LIBRFID_FIRMWARE
extern void debugp(const char *format, ...);
extern const char *hexdump(const void *data, unsigned int len);
#define rfid_hexdump(x, y) hexdump(x, y)
#define DEBUGP(x, args ...) debugp("%s(%d):%s: " x, __FILE__, __LINE__, __FUNCTION__, ## args)
#define DEBUGPC(x, args ...) debugp(x, ## args)
#else /* LIBRFID_FIRMWARE */
extern const char *rfid_hexdump(const void *data, unsigned int len);
#define DEBUGP(x, args ...) fprintf(stderr, "%s(%d):%s: " x, __FILE__, __LINE__, __FUNCTION__, ## args)
#define DEBUGPC(x, args ...) fprintf(stderr, x, ## args)
#endif /* LIBRFID_FIRMWARE */

#else /* DEBUG */
extern const char *rfid_hexdump(const void *data, unsigned int len);

#define DEBUGP(x, args ...)
#define DEBUGPC(x, args ...)

#endif /* DEBUG */


#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#endif /* __LIBRFID__ */

int rfid_init();

enum rfid_opt_level {
	RFID_LEVEL_ASIC,
	RFID_LEVEL_READER,
	RFID_LEVEL_LAYER2,
	RFID_LEVEL_LAYER3,
};

#endif /* _RFID_H */
