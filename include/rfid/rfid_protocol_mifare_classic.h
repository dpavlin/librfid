#ifndef _MIFARE_CLASSIC_H

extern struct rfid_protocol rfid_protocol_mfcl;

#define RFID_CMD_MIFARE_AUTH1A	0x60
#define RFID_CMD_MIFARE_AUTH1B	0x61

#define MIFARE_CLASSIC_KEY_DEFAULT	"\xa0\xa1\xa2\xa3\xa4\xa5"


#endif
