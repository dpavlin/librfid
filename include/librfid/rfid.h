#ifndef _RFID_H
#define _RFID_H

#include <stddef.h>
#include <sys/types.h>
#include <stdio.h>

#define ECOLLISION		1000

#ifdef  __MINGW32__
#define ENOTSUP         100
/* 110 under linux */
#define ETIMEDOUT       101

typedef unsigned char u_int8_t;
typedef unsigned short u_int16_t;
typedef unsigned long u_int32_t;
typedef unsigned long long u_int64_t;
#endif/*__MINGW32__*/

#ifdef  __LIBRFID__

#include <librfid/rfid_system.h>

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

#include <stdio.h>

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

enum rfid_frametype {
	RFID_14443A_FRAME_REGULAR,
	RFID_14443B_FRAME_REGULAR,
	RFID_MIFARE_FRAME,
	RFID_15693_FRAME,
	RFID_15693_FRAME_ICODE1,
};

#endif /* _RFID_H */
