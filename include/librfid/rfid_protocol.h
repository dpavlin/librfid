#ifndef _RFID_PROTOCOL_H
#define _RFID_PROTOCOL_H

#include <librfid/rfid_layer2.h>

struct rfid_protocol_handle;

struct rfid_protocol_handle *
rfid_protocol_init(struct rfid_layer2_handle *l2h, unsigned int id);
int rfid_protocol_open(struct rfid_protocol_handle *ph);
int rfid_protocol_transceive(struct rfid_protocol_handle *ph,
			     const unsigned char *tx_buf, unsigned int tx_len,
			     unsigned char *rx_buf, unsigned int *rx_len,
			     unsigned int timeout, unsigned int flags);
int
rfid_protocol_read(struct rfid_protocol_handle *ph,
	 	   unsigned int page,
		   unsigned char *rx_data,
		   unsigned int *rx_len);

int
rfid_protocol_write(struct rfid_protocol_handle *ph,
	 	   unsigned int page,
		   unsigned char *tx_data,
		   unsigned int tx_len);

int rfid_protocol_fini(struct rfid_protocol_handle *ph);
int rfid_protocol_close(struct rfid_protocol_handle *ph);

int rfid_protocol_getopt(struct rfid_protocol_handle *ph, int optname,
			 void *optval, unsigned int *optlen);

int rfid_protocol_setopt(struct rfid_protocol_handle *ph, int optname,
			 const void *optval, unsigned int optlen);

char *rfid_protocol_name(struct rfid_protocol_handle *ph);

enum rfid_protocol_id {
	RFID_PROTOCOL_UNKNOWN,
	RFID_PROTOCOL_TCL,
	RFID_PROTOCOL_MIFARE_UL,
	RFID_PROTOCOL_MIFARE_CLASSIC,
	RFID_PROTOCOL_ICODE_SLI,
	RFID_PROTOCOL_TAGIT,
	NUM_RFID_PROTOCOLS
};

enum rfid_protocol_opt {
	RFID_OPT_PROTO_ID,
	RFID_OPT_PROTO_SIZE	= 0x10000001,
};

#ifdef __LIBRFID__

struct rfid_protocol {
	unsigned int id;
	char *name;
	struct {
		struct rfid_protocol_handle *(*init)(struct rfid_layer2_handle *l2h);
		int (*open)(struct rfid_protocol_handle *ph);
		int (*close)(struct rfid_protocol_handle *ph);
		int (*fini)(struct rfid_protocol_handle *ph);
		/* transceive for session based transport protocols */
		int (*transceive)(struct rfid_protocol_handle *ph,
				  const unsigned char *tx_buf,
				  unsigned int tx_len,
				  unsigned char *rx_buf,
				  unsigned int *rx_len,
				  unsigned int timeout,
				  unsigned int flags);
		/* read/write for synchronous memory cards */
		int (*read)(struct rfid_protocol_handle *ph,
			    unsigned int page,
			    unsigned char *rx_data,
			    unsigned int *rx_len);
		int (*write)(struct rfid_protocol_handle *ph,
			     unsigned int page,
			     unsigned char *tx_data,
			     unsigned int tx_len);
		int (*getopt)(struct rfid_protocol_handle *h,
			      int optname, void *optval, unsigned int *optlen);
		int (*setopt)(struct rfid_protocol_handle *h,
			      int optname, const void *optval,
			      unsigned int optlen);
	} fn;
};

#include <librfid/rfid_protocol_tcl.h>
#include <librfid/rfid_protocol_mifare_ul.h>
#include <librfid/rfid_protocol_mifare_classic.h>
#include <librfid/rfid_protocol_tagit.h>

struct rfid_protocol_handle {
	struct rfid_layer2_handle *l2h;
	const struct rfid_protocol *proto;
	union {
		struct tcl_handle tcl;
	} priv;				/* priv has to be last, since
					 * it could contain additional
					 * private data over the end of
					 * sizeof(priv). */
};

#endif /* __LIBRFID__ */

#endif /* _RFID_PROTOCOL_H */
