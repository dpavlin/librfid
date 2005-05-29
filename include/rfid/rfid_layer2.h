#ifndef _RFID_LAYER2_H
#define _RFID_LAYER2_H

struct rfid_layer2_handle;
struct rfid_reader_handle;

#include <rfid/rfid_layer2_iso14443a.h>
#include <rfid/rfid_layer2_iso14443b.h>


struct rfid_layer2 {
	unsigned int id;
	char *name;

	struct {
		struct rfid_layer2_handle *(*init)(struct rfid_reader_handle *h);
		int (*open)(struct rfid_layer2_handle *h);
		int (*transcieve)(struct rfid_layer2_handle *h,
				  const unsigned char *tx_buf, 
				  unsigned int tx_len, unsigned char *rx_buf, 
				  unsigned int *rx_len, unsigned int timeout, 
				  unsigned int flags);
		int (*close)(struct rfid_layer2_handle *h);
		int (*fini)(struct rfid_layer2_handle *h);
	} fn;
	struct rfid_layer2 *next;
};

struct rfid_layer2_handle {
	struct rfid_reader_handle *rh;
	union {
		struct iso14443a_handle iso14443a;
		struct iso14443b_handle iso14443b;
		//struct iso15693_handle iso15693;
	} priv;
	struct rfid_layer2 *l2;
};

enum rfid_layer2_id {
	RFID_LAYER2_NONE,
	RFID_LAYER2_ISO14443A,
	RFID_LAYER2_ISO14443B,
	RFID_LAYER2_ISO15693,
};

struct rfid_layer2_handle *rfid_layer2_init(struct rfid_reader_handle *rh,
					    unsigned int id);
int rfid_layer2_open(struct rfid_layer2_handle *l2h);
int rfid_layer2_transcieve(struct rfid_layer2_handle *l2h,
			   const unsigned char *tx_buf, unsigned int tx_len,
			   unsigned char *rx_buf, unsigned int *rx_len,
			   unsigned int timeout, unsigned int flags);
int rfid_layer2_close(struct rfid_layer2_handle *l2h);
int rfid_layer2_fini(struct rfid_layer2_handle *l2h);

#endif
