#ifndef _RFID_PROTOCOL_MFUL_H
#define _RFID_PROTOCOL_MFUL_H


#define MIFARE_UL_CMD_WRITE	0xA2
#define MIFARE_UL_CMD_READ	0x30

#define MIFARE_UL_RESP_ACK	0x0a
#define MIFARE_UL_RESP_NAK	0x00

#define MIFARE_UL_PAGE_MAX	15

struct rfid_protocol rfid_protocol_mful;

#endif
