#ifndef _RFID_H
#define _RFID_H

#include <stdio.h>

#ifdef __LIBRFID__

enum rfid_frametype {
	RFID_14443A_FRAME_REGULAR,
	RFID_14443B_FRAME_REGULAR,
	RFID_MIFARE_FRAME,
};

#if 1
#define DEBUGP(x, args ...) fprintf(stderr, "%s(%d):%s: " x, __FILE__, __LINE__, __FUNCTION__, ## args)
#define DEBUGPC(x, args ...) fprintf(stderr, x, ## args)
#else
#define DEBUGP(x, args ...)
#define DEBUGPC(x, args ...)
#endif

extern const char *rfid_hexdump(const void *data, unsigned int len);

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#endif /* __LIBRFID__ */

int rfid_init();

#endif /* _RFID_H */
