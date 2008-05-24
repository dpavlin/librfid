#ifndef _RFID_ISO15693_H
#define _RFID_ISO15693_H

#include <sys/types.h>

/*
 * ISO15693 tag manufacturer codes as found at
 * http://rfid-handbook.de/forum/read.php?5,437,580#msg-580
 *
 * 01h = Motorola
 * 02h = ST Microelectronics
 * 03h = Hitachi
 * 04h = Philips/NXP I.CODE
 * 05h = Siemens/Infineon
 * 06h = Cylinc
 * 07h = Texas Instruments TagIt
 * 08h = Fujitsu Limited
 * 09h = Mashushita Electric Industrial
 * 0Ah = NEC
 * 0Bh = Oki Electric
 * 0Ch = Toshiba
 * 0Dh = Mishubishi Electric
 * 0Eh = Samsung Electronics
 * 0Fh = Hyundai Electronics
 * 10h = LG Semiconductors
 * 16h = EMarin Microelectronic
 *
 */

/* protocol definitions */

struct iso15693_handle {
	unsigned int state;
	unsigned int vcd_ask100:1,
		     vicc_two_subc:1,
		     vicc_fast:1,
		     single_slot:1,
		     use_afi:1,
		     vcd_out256:1;
	u_int8_t afi;	/* appplication family identifier */
	u_int8_t dsfid;	/* data storage format identifier */
};

enum rfid_15693_state {
	ISO15693_STATE_ERROR,
	ISO15693_STATE_NONE,
	ISO15693_STATE_ANTICOL_RUNNING,
};

enum rfid_15693_opt {
	RFID_OPT_15693_MOD_DEPTH	= 0x00010001,
	RFID_OPT_15693_VCD_CODING	= 0x00010002,
	RFID_OPT_15693_VICC_SUBC	= 0x00010003,
	RFID_OPT_15693_VICC_SPEED	= 0x00010004,
	RFID_OPT_15693_VCD_SLOTS	= 0x00010005,
	RFID_OPT_15693_USE_AFI		= 0x00010006,
	RFID_OPT_15693_AFI		= 0x00010007,
	RFID_OPT_15693_DSFID		= 0x00010008,
};

enum rfid_15693_opt_mod_depth {
	RFID_15693_MOD_10ASK	= 0x01,
	RFID_15693_MOD_100ASK	= 0x02,
};

enum rfid_15693_opt_vcd_coding {
	RFID_15693_VCD_CODING_1OUT256	= 0x01,
	RFID_15693_VCD_CODING_1OUT4	= 0x02,
};

enum rfid_15693_opt_vicc_subc {
	RFID_15693_VICC_SUBC_SINGLE	= 0x01,
	RFID_15693_VICC_SUBC_DUAL	= 0x02,
};

enum rfid_15693_opt_vicc_speed {
	RFID_15693_VICC_SPEED_SLOW	= 0x01,
	RFID_15693_VICC_SPEED_FAST	= 0x02,
};

#ifdef __LIBRFID__

#define ISO15693_UID_LEN	8
#define ISO15693_CRC_LEN	2

/* ISO 15693-3, Ch. 7.2 Table 3 */
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

/* ISO 15963-3, Ch. 7.2 Figure 5 */
struct iso15693_response {
	u_int8_t flags;
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
    RFID_15693_ERR_NOTSUPP_OPTION = 0x03, /* option not supported*/
	RFID_15693_ERR_UNKNOWN	= 0x0f, /* unknown error */
	RFID_15693_ERR_BLOCK_NA	= 0x10, /* block not available */
	RFID_15693_ERR_BLOCK_LOCKED = 0x11,
	RFID_15693_ERR_BLOCK_LOCKED_CH = 0x12,
	RFID_15693_ERR_BLOCK_NOTPROG = 0x13,
	RFID_15693_ERR_BLOCK_NOTLOCK = 0x14,
	/* 0xA0 .. 0xDF Custom Command error Codes */
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
	ISO15693_CMD_GET_BLOCK_SECURITY	= 0x2c,
	/* Custom 0xa0 .. 0xdf */
	ICODE_CMD_INVENTORY_READ	= 0xa0,
	ICODE_CMD_FAST_INVENTORY_READ	= 0xa1,
	ICODE_CMD_EAS_SET		= 0xa2,
	ICODE_CMD_EAS_RESET		= 0xa3,
	ICODE_CMD_EAS_LOCK		= 0xa4,
	ICODE_CMD_EAS_ALARM		= 0xa5,
	/* Proprietary 0xe0 .. 0xff */
};

struct iso15693_anticol_cmd {
	struct iso15693_request req;
	unsigned char mask_len;
	unsigned char mask_bits[ISO15693_UID_LEN];
	unsigned char current_slot;
} __attribute__((packed));

struct iso15693_anticol_cmd_afi {
	struct iso15693_request req;
	unsigned char afi;
	unsigned char mask_len;
	unsigned char mask_bits[ISO15693_UID_LEN];
} __attribute__((packed));

/* Figure 11, Chapter 9.2.1 */
struct iso15693_anticol_resp {
	struct iso15693_response resp;
	u_int8_t dsfid;
	u_int8_t uuid[ISO15693_UID_LEN];
} __attribute__((packed));


#define ISO15693_T_SLOW	0
#define ISO15693_T_FAST	1
enum iso15693_t {
	ISO15693_T1,
	ISO15693_T2,
	ISO15693_T3,
	ISO15693_T4,
	ISO15693_T4_WRITE,
};

/* in microseconds as per Chapter 8.4 table 8 */
extern const unsigned int iso15693_timing[2][5];

#include <librfid/rfid_layer2.h>
extern const struct rfid_layer2 rfid_layer2_iso15693;

#endif /* __LIBRFID__ */
#endif /* _ISO15693_H */
