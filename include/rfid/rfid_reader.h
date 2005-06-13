#ifndef _RFID_READER_H
#define _RFID_READER_H

#include <rfid/rfid_asic.h>
#include <rfid/rfid_layer2_iso14443a.h>

struct rfid_reader_handle;

struct rfid_reader {
	char *name;
	unsigned int id;
	int (*transcieve)(struct rfid_reader_handle *h,
			  const unsigned char *tx_buf, unsigned int tx_len,
			  unsigned char *rx_buf, unsigned int *rx_len,
			  unsigned int timeout, unsigned int flags);
	struct rfid_reader_handle * (*open)(void *data);
	int (*close)(void *data);

	struct rfid_14443a_reader {
		int (*init)(struct rfid_reader_handle *h);
		int (*transcieve_sf)(struct rfid_reader_handle *h,
				     unsigned char cmd,
				     struct iso14443a_atqa *atqa);
		int (*transcieve_acf)(struct rfid_reader_handle *h,
				      struct iso14443a_anticol_cmd *cmd,
				      unsigned int *bit_of_col);
		unsigned int speed;
	} iso14443a;
	struct rfid_14443b_reader {
		int (*init)(struct rfid_reader_handle *rh);
		unsigned int speed;
	} iso14443b;
	struct rfid_15693_reader {

	} iso15693;
	struct rfid_reader *next;
};

enum rfid_reader_id {
	RFID_READER_CM5121,
	RFID_READER_PEGODA,
};

enum rfid_reader_14443a_speed {
	RFID_READER_SPEED_106K,
	RFID_READER_SPEED_212K,
	RFID_READER_SPEED_424K,
	RFID_READER_SPEED_848K,
};

struct rfid_reader_handle {
	struct rfid_asic_handle *ah;

	union {

	} priv;
	struct rfid_reader *reader;
};
#endif
