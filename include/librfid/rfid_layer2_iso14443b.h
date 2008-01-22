#ifndef _RFID_LAYER2_ISO14443B_H
#define _RFID_LAYER2_ISO14443B_H

#include <librfid/rfid_layer2.h>

enum rfid_iso14443_opt {
	RFID_OPT_14443B_CID		= 0x00010001,
	RFID_OPT_14443B_FSC		= 0x00010002,
	RFID_OPT_14443B_FSD		= 0x00010003,
	RFID_OPT_14443B_FWT		= 0x00010004,
	RFID_OPT_14443B_TR0		= 0x00010005,
	RFID_OPT_14443B_TR1		= 0x00010006,
};

#ifdef __LIBRFID__

struct iso14443b_atqb {
	unsigned char fifty;
	unsigned char pupi[4];
	unsigned char app_data[4];
	struct {
		unsigned char bit_rate_capability;
#ifndef RFID_BIG_ENDIAN_BITFIELD
		unsigned char protocol_type:4,
			      max_frame_size:4;
		unsigned char fo:2,
			      adc:2,
			      fwi:4;
#else
		unsigned char max_frame_size:4,
			      protocol_type:4;
		unsigned char fwi:4,
			      adc:2,
			      fo:2;
#endif
	} __attribute__ ((packed)) protocol_info;
} __attribute__ ((packed));

struct iso14443b_attrib_hdr {
	unsigned char one_d;
	unsigned char identifier[4];
	struct {
#ifndef RFID_BIG_ENDIAN_BITFIELD
		unsigned char rfu:2,
			      sof:1,
			      eof:1,
			      min_tr1:2,
			      min_tr0:2;
#else
		unsigned char min_tr0:2,
			      min_tr1:1,
			      eof:1,
			      sof:1,
			      rfu:2;
#endif
	} __attribute__ ((packed)) param1;
	struct {
#ifndef RFID_BIG_ENDIAN_BITFIELD
		unsigned char fsdi:4,
			      spd_out:2,
			      spd_in:2;
#else
		unsigned char spd_in:2,
			      spd_out:2,
			      fsdi:4;
#endif
	} __attribute__ ((packed)) param2;
	struct {
#ifndef RFID_BIG_ENDIAN_BITFIELD
		unsigned char protocol_type:4,
			      rfu:4;
#else
		unsigned char rfu:4,
			      protocol_type:4;
#endif
	} __attribute__ ((packed)) param3;
	struct {
#ifndef RFID_BIG_ENDIAN_BITFIELD
		unsigned char cid:4,
			      rfu:4;
#else
		unsigned char rfu:4,
			      cid:4;
#endif
	} __attribute__ ((packed)) param4;
} __attribute__ ((packed));

struct iso14443b_handle {
	unsigned int tcl_capable; /* do we support T=CL */

	unsigned int cid;	/* Card ID */

	unsigned int fsc;	/* max. frame size card */
	unsigned int fsd;	/* max. frame size reader */

	unsigned int fwt;	/* frame waiting time (in usec) */

	unsigned int mbl;	/* maximum buffer length */

	unsigned int tr0;	/* pcd-eof to picc-subcarrier-on */
	unsigned int tr1;	/* picc-subcarrier-on to picc-sof */

	unsigned int flags;
	unsigned int state;
};

enum {
	ISO14443B_CID_SUPPORTED = 0x01,
	ISO14443B_NAD_SUPPORTED = 0x02,
};

enum {
	ISO14443B_STATE_ERROR,
	ISO14443B_STATE_NONE,
	ISO14443B_STATE_REQB_SENT,
	ISO14443B_STATE_ATQB_RCVD,
	ISO14443B_STATE_ATTRIB_SENT,
	ISO14443B_STATE_SELECTED,
	ISO14443B_STATE_HLTB_SENT,
	ISO14443B_STATE_HALTED,
};

#include <librfid/rfid_layer2.h>
extern const struct rfid_layer2 rfid_layer2_iso14443b;

#endif /* __LIBRFID__ */

#endif
