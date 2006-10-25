#ifndef _RFID_ISO15693_H
#define _RFID_ISO15693_H

#include <sys/types.h>

/*
07h = TagIt
04h = I.CODE
05h = Infineon
02h = ST
*/

/* protocol definitions */

struct iso15693_handle;

struct iso15693_transport {
	unsigned char	*name;

	struct {
		int (*init)(struct iso15693_handle *handle);
		int (*fini)(struct iso15693_handle *handle);

#if 0
		int (*transceive_sf)(struct iso14443a_handle *handle,
				     unsigned char cmd,
				     struct iso14443a_atqa *atqa);
		int (*transceive_acf)(struct iso14443a_handle *handle,
				      struct iso14443a_anticol_cmd *acf,
				      unsigned int *bit_of_col);
#endif
		int (*transceive)(struct iso15693_handle *handle,
				  const unsigned char *tx_buf,
				  unsigned int tx_len,
				  unsigned char *rx_buf,
				  unsigned int *rx_len);
	} fn;

	union {
	} priv;
};

struct iso15693_handle {
	unsigned int state;
};

enum iso15693_state {
	ISO15693_STATE_ERROR,
	ISO15693_STATE_NONE,
};

#ifdef __LIBRFID__

#define ISO15693_UID_LEN	8
#define ISO15693_CRC_LEN	2

/* ISO 15693-3, Ch. 7.2 Table 3*/
enum iso15693_request_flags {
	RFID_15693_F_SUBC_TWO	 = 0x01,
	RFID_15693_F_RATE_HIGH	 = 0x02,
	RFID_15693_F_INV_TABLE_5 = 0x04,
	RFID_15693_F_PROT_OEXT	 = 0x08,
};

/* ISO 15693-3, Ch. 7.2 Table 4 */
enum iso15693_request_flags_table4 {
	RFID_15693_F4_SELECTED	=  0x10, /* only VICC in 'select' state */
	RFID_15693_F4_ADDRESS	=  0x20, /* request is addressed */
	RFID_15693_F4_CUSTOM	=  0x40,
};

/* ISO 15693-3, Ch. 7.2 Table 5 */
enum iso15693_request_flags_table5 {
	RFID_15693_F5_AFI_PRES	=  0x10, /* AFI is present */
	RFID_15693_F5_NSLOTS_1	=  0x20, /* only 1 slot (instead of 16) */
	RFID_15693_F5_CUSTOM	=  0x40,
};

/* ISO 15963-3, Ch. 7.2 Figure 4 */
struct iso15693_request {
	u_int8_t flags;
	u_int8_t command;
	u_int8_t data[0];
} __attribute__ ((packed));



/* ISO 15693, Ch. 7.3 Table 6 */
enum iso15693_response_flags {
	RFID_15693_RF_ERROR	= 0x01,
	RFID_15693_RF_EXTENDED	= 0x08,
};

/* ISO 15693, Ch. 7.3.2 Table 7 */
enum iso15693_response_errors {
	RFID_15693_ERR_NOTSUPP	= 0x01,
	RFID_15693_ERR_INVALID	= 0x02, /* command not recognized */
	RFID_15693_ERR_UNKNOWN	= 0x0f, /* unknown error */
	RFID_15693_ERR_BLOCK_NA	= 0x10, /* block not available */
	RFID_15693_ERR_BLOCK_LOCKED = 0x11,
	RFID_15693_ERR_BLOCK_LOCKED_CH = 0x12,
	RFID_15693_ERR_BLOCK_NOTPROG = 0x13,
	RFID_15693_ERR_BLOCK_NOTLOCK = 0x14,
};

/* ISO 15693, Ch. 7.4 */
enum iso15693_vicc_states {
	RFID_15693_STATE_POWER_OFF,
	RFID_15693_STATE_READY,
	RFID_15693_STATE_QUIET,
	RFID_15693_STATE_SELECTED,
};

/* ISO 15693, Ch. 9.1 Table 10*/
enum iso15693_commands {
	/* Mandatory 0x01 .. 0x1f */
	ISO15693_CMD_INVENTORY		= 0x01,
	ISO15693_CMD_STAY_QUIET		= 0x02,
	/* Optional 0x20 .. 0x9f */
	ISO15693_CMD_READ_BLOCK_SINGLE	= 0x20,
	ISO15693_CMD_WRITE_BLOCK_SINGLE	= 0x21,
	ISO15693_CMD_LOCK_BLOCK		= 0x22,
	ISO15693_CMD_READ_BLOCK_MULTI	= 0x23,
	ISO15693_CMD_WRITE_BLOCK_MULTI	= 0x24,
	ISO15693_CMD_SELECT		= 0x25,
	ISO15693_CMD_RESET_TO_READY	= 0x26,
	ISO15693_CMD_WRITE_AFI		= 0x27,
	ISO15693_CMD_LOCK_AFI		= 0x28,
	ISO15693_CMD_WRITE_DSFID	= 0x29,
	ISO15693_CMD_LOCK_DSFID		= 0x2a,
	ISO15693_CMD_GET_SYSINFO	= 0x2b,
	ISO15693_CMD_GET_BLOCK_SECURITY	= 0x2c
	/* Custom 0xa0 .. 0xdf */
	/* Proprietary 0xe0 .. 0xff */
};


#include <librfid/rfid_layer2.h>
extern const struct rfid_layer2 rfid_layer2_iso15693;

#endif /* __LIBRFID__ */
#endif /* _ISO15693_H */
