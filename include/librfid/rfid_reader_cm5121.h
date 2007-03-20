#ifndef _RFID_READER_CM5121_H
#define _RFID_READER_CM5121_H

#include <librfid/rfid_reader.h>

#define CM5121_CW_CONDUCTANCE	0x3f
#define CM5121_MOD_CONDUCTANCE	0x3f
#define CM5121_14443A_BITPHASE	0xa9
#define CM5121_14443A_THRESHOLD	0xff

#define CM5121_14443B_BITPHASE	0xad
#define CM5121_14443B_THRESHOLD	0xff

extern int
PC_to_RDR_Escape(void *handle,
		const unsigned char *tx_buf, size_t tx_len,
		unsigned char *rx_buf, size_t *rx_len);

extern const struct rfid_reader rfid_reader_cm5121;
// extern struct rfid_asic_transport cm5121_ccid;

#endif
