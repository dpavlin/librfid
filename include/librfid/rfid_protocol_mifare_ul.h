#ifndef _RFID_PROTOCOL_MFUL_H
#define _RFID_PROTOCOL_MFUL_H

int rfid_mful_lock_page(struct rfid_protocol_handle *ph, unsigned int page);
int rfid_mful_lock_otp(struct rfid_protocol_handle *ph);

#define MIFARE_UL_PAGE_MAX	15

#ifdef __LIBRFID__

#define MIFARE_UL_CMD_WRITE	0xA2
#define MIFARE_UL_CMD_READ	0x30

#define MIFARE_UL_RESP_ACK	0x0a
#define MIFARE_UL_RESP_NAK	0x00

#define MIFARE_UL_PAGE_LOCK	2
#define MIFARE_UL_PAGE_OTP	3

extern const struct rfid_protocol rfid_protocol_mful;

#endif /* __LIBRFID__ */

#endif
