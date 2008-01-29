#ifndef _RFID_LAYER2_H
#define _RFID_LAYER2_H

#include <librfid/rfid.h>

struct rfid_layer2_handle;
struct rfid_reader_handle;

enum rfid_layer2_id {
	RFID_LAYER2_NONE,
	RFID_LAYER2_ISO14443A,
	RFID_LAYER2_ISO14443B,
	RFID_LAYER2_ISO15693,
	RFID_LAYER2_ICODE1,
};

/* 0...0xffff = global options, 0x10000...0x1ffff = private options */
#define RFID_OPT_L2_PRIV		0x00010000
enum rfid_layer2_opt {
	RFID_OPT_LAYER2_UID		= 0x0001,
	RFID_OPT_LAYER2_PROTO_SUPP	= 0x0002,
	RFID_OPT_LAYER2_WUP		= 0x0003,
};

struct rfid_layer2_handle *rfid_layer2_init(struct rfid_reader_handle *rh,
					    unsigned int id);
int rfid_layer2_open(struct rfid_layer2_handle *l2h);
int rfid_layer2_transceive(struct rfid_layer2_handle *l2h,
			   enum rfid_frametype frametype,
			   const unsigned char *tx_buf, unsigned int tx_len,
			   unsigned char *rx_buf, unsigned int *rx_len,
			   u_int64_t timeout, unsigned int flags);
int rfid_layer2_close(struct rfid_layer2_handle *l2h);
int rfid_layer2_fini(struct rfid_layer2_handle *l2h);
int rfid_layer2_getopt(struct rfid_layer2_handle *l2h, int optname,
			void *optval, unsigned int *optlen);
int rfid_layer2_setopt(struct rfid_layer2_handle *l2h, int optname,
			const void *optval, unsigned int optlen);
char *rfid_layer2_name(struct rfid_layer2_handle *l2h);
#ifdef __LIBRFID__

#include <librfid/rfid_layer2_iso14443a.h>
#include <librfid/rfid_layer2_iso14443b.h>
#include <librfid/rfid_layer2_iso15693.h>
#include <librfid/rfid_layer2_icode1.h>

struct rfid_layer2 {
	unsigned int id;
	char *name;

	struct {
		struct rfid_layer2_handle *(*init)(struct rfid_reader_handle *h);
		int (*open)(struct rfid_layer2_handle *h);
		int (*transceive)(struct rfid_layer2_handle *h,
				  enum rfid_frametype frametype,
				  const unsigned char *tx_buf, 
				  unsigned int tx_len, unsigned char *rx_buf, 
				  unsigned int *rx_len, u_int64_t timeout,
				  unsigned int flags);
		int (*close)(struct rfid_layer2_handle *h);
		int (*fini)(struct rfid_layer2_handle *h);
		int (*getopt)(struct rfid_layer2_handle *h,
			      int optname, void *optval, unsigned int *optlen);
		int (*setopt)(struct rfid_layer2_handle *h,
			      int optname, const void *optval,
			      unsigned int optlen);
	} fn;
};

struct rfid_layer2_handle {
	struct rfid_reader_handle *rh;
	unsigned char uid[10];	/* triple size 14443a id is 10 bytes */
	unsigned int uid_len;
	unsigned int proto_supported;
	unsigned int flags;
	union {
		struct iso14443a_handle iso14443a;
		struct iso14443b_handle iso14443b;
		struct iso15693_handle iso15693;
	} priv;
	const struct rfid_layer2 *l2;
};

#endif /* __LIBRFID__ */

#endif
