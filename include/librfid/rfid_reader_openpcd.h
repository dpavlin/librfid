#ifndef _OPENPCD_PROTO_H
#define _OPENPCD_PROTO_H

/* This header file describes the USB protocol of the OpenPCD RFID reader */

#include <sys/types.h>

struct openpcd_hdr {
	u_int8_t cmd;		/* command */
	u_int8_t flags;
	u_int8_t reg;		/* register */
	u_int8_t val;		/* value (in case of write *) */
	u_int16_t len;
	u_int16_t res;
	u_int8_t data[0];
} __attribute__((packed));

#define OPENPCD_REG_MAX			0x3f

#define OPENPCD_CMD_WRITE_REG		0x01
#define OPENPCD_CMD_WRITE_FIFO		0x02
#define OPENPCD_CMD_WRITE_VFIFO		0x03
#define OPENPCD_CMD_REG_BITS_CLEAR	0x04
#define OPENPCD_CMD_REG_BITS_SET	0x05

#define OPENPCD_CMD_READ_REG		0x11
#define OPENPCD_CMD_READ_FIFO		0x12
#define OPENPCD_CMD_READ_VFIFO		0x13

#define OPENPCD_CMD_SET_LED		0x21

#define OPENPCD_CMD_IRQ			0x40	/* IRQ reported by RC632 */

#define OPENPCD_VENDOR_ID	0x2342
#define OPENPCD_PRODUCT_ID	0x0001
#define OPENPCD_OUT_EP		0x01
#define OPENPCD_IN_EP		0x82
#define OPENPCD_IRQ_EP		0x83

extern struct rfid_reader rfid_reader_openpcd;

#endif
