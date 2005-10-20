#ifndef _RFID_ISO14443A_H
#define _RFID_ISO14443A_H

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
	u_int8_t bf_anticol:5,
		 rfu1:1,
		 uid_size:2;
	u_int8_t proprietary:4,
		 rfu2:4;
};

#define ISO14443A_HLTA		0x5000

/* ISO 14443-3, Chapter 6.3.2 */
struct iso14443a_anticol_cmd {
	unsigned char		sel_code;
	unsigned char		nvb;
	unsigned char		uid_bits[5];
};

enum iso14443a_anticol_sel_code {
	ISO14443A_AC_SEL_CODE_CL1	= 0x93,
	ISO14443A_AC_SEL_CODE_CL2	= 0x95,
	ISO14443A_AC_SEL_CODE_CL3	= 0x97,
};

#define	ISO14443A_BITOFCOL_NONE		0xffffffff

struct iso14443a_handle;

struct iso14443a_transport {
	unsigned char	*name;

	struct {
		int (*init)(struct iso14443a_handle *handle);
		int (*fini)(struct iso14443a_handle *handle);

		int (*transcieve_sf)(struct iso14443a_handle *handle,
				     unsigned char cmd,
				     struct iso14443a_atqa *atqa);
		int (*transcieve_acf)(struct iso14443a_handle *handle,
				      struct iso14443a_anticol_cmd *acf,
				      unsigned int *bit_of_col);
		int (*transcieve)(struct iso14443a_handle *handle,
				  const unsigned char *tx_buf,
				  unsigned int tx_len,
				  unsigned char *rx_buf,
				  unsigned int *rx_len);
	} fn;

	union {
	} priv;
};

struct iso14443a_handle {
	unsigned int state;
	unsigned int level;
	unsigned int tcl_capable;
	unsigned int uid_len;
	unsigned char uid[10];		/* Triple size UID is 10 bytes */
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

#include <rfid/rfid_layer2.h>
struct rfid_layer2 rfid_layer2_iso14443a;

#endif /* _ISO14443A_H */
