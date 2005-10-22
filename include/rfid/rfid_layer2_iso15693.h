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
		int (*transcieve_sf)(struct iso14443a_handle *handle,
				     unsigned char cmd,
				     struct iso14443a_atqa *atqa);
		int (*transcieve_acf)(struct iso14443a_handle *handle,
				      struct iso14443a_anticol_cmd *acf,
				      unsigned int *bit_of_col);
#endif
		int (*transcieve)(struct iso15693_handle *handle,
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

#include <rfid/rfid_layer2.h>
extern struct rfid_layer2 rfid_layer2_iso15693;

#endif /* _ISO15693_H */
