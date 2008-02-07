#ifndef _RFID_ISO14443A_H
#define _RFID_ISO14443A_H

enum rfid_14443a_opt {
	RFID_OPT_14443A_SPEED_RX	= 0x00010001,
	RFID_OPT_14443A_SPEED_TX	= 0x00010002,
	RFID_OPT_14443A_ATQA		= 0x00010003,
	RFID_OPT_14443A_WUPA		= 0x00010004,
	RFID_OPT_14443A_SAK		= 0x00010005,
};

enum rfid_14443_opt_speed {
	RFID_14443A_SPEED_106K	= 0x01,
	RFID_14443A_SPEED_212K	= 0x02,
	RFID_14443A_SPEED_424K  = 0x04,
	RFID_14443A_SPEED_848K  = 0x08,
};

#ifdef __LIBRFID__

#include <sys/types.h>

/* protocol definitions */

/* ISO 14443-3, Chapter 6.3.1 */
enum iso14443a_sf_cmd {
	ISO14443A_SF_CMD_REQA		= 0x26,
	ISO14443A_SF_CMD_WUPA		= 0x52,
	ISO14443A_SF_CMD_OPT_TIMESLOT	= 0x35,		/* Annex C */
	/* 40 to 4f and 78 to 7f: proprietary */
};

struct iso14443a_atqa {
#ifndef RFID_BIG_ENDIAN_BITFIELD
	u_int8_t bf_anticol:5,
		 rfu1:1,
		 uid_size:2;
	u_int8_t proprietary:4,
		 rfu2:4;
#else
	u_int8_t uid_size:2,
		 rfu1:1,
		 bf_anticol:5;
	u_int8_t rfu2:4,
		 proprietary:4;
#endif
} __attribute__((packed));

#define ISO14443A_HLTA		0x5000

/* ISO 14443-3, Chapter 6.3.2 */
struct iso14443a_anticol_cmd {
	unsigned char		sel_code;
	unsigned char		nvb;
	unsigned char		uid_bits[5];
} __attribute__((packed));

enum iso14443a_anticol_sel_code {
	ISO14443A_AC_SEL_CODE_CL1	= 0x93,
	ISO14443A_AC_SEL_CODE_CL2	= 0x95,
	ISO14443A_AC_SEL_CODE_CL3	= 0x97,
};

#define	ISO14443A_BITOFCOL_NONE		0xffffffff

struct iso14443a_handle {
	unsigned int state;
	unsigned int level;
	unsigned int tcl_capable;
	struct iso14443a_atqa atqa;
	u_int8_t sak;
};

enum iso14443a_level {
	ISO14443A_LEVEL_NONE,
	ISO14443A_LEVEL_CL1,
	ISO14443A_LEVEL_CL2,
	ISO14443A_LEVEL_CL3,
};

enum iso14443a_state {
	ISO14443A_STATE_ERROR,
	ISO14443A_STATE_NONE,
	ISO14443A_STATE_REQA_SENT,
	ISO14443A_STATE_ATQA_RCVD,
	ISO14443A_STATE_NO_BITFRAME_ANTICOL,
	ISO14443A_STATE_ANTICOL_RUNNING,
	ISO14443A_STATE_SELECTED,
};

/* Section 6.1.2 values in usec, rounded up to next usec */
#define ISO14443A_FDT_ANTICOL_LAST1	92	/* 1236 / fc = 91.15 usec */
#define ISO14443A_FDT_ANTICOL_LAST0	87	/* 1172 / fc = 86.43 usec */

#define ISO14443_CARRIER_FREQ	13560000
#define ISO14443A_FDT_OTHER_LAST1(n)	(((n*128+84)*1000000)/ISO14443_CARRIER_FREQ)


#include <librfid/rfid_layer2.h>
extern const struct rfid_layer2 rfid_layer2_iso14443a;

#endif /* __LIBRFID__ */


#endif /* _ISO14443A_H */
