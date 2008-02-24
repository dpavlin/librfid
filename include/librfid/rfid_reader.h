#ifndef _RFID_READER_H
#define _RFID_READER_H

#include <librfid/rfid_asic.h>
#include <librfid/rfid_layer2_iso14443a.h>
#include <librfid/rfid_layer2_iso15693.h>

struct rfid_reader_handle;

/* 0...0xffff = global options, 0x10000...0x1ffff = private options */
#define RFID_OPT_RDR_PRIV		0x00010000
enum rfid_reader_opt {
	RFID_OPT_RDR_FW_VERSION		= 0x0001,
	RFID_OPT_RDR_RF_KILL		= 0x0002,
};


struct rfid_reader {
	char *name;
	unsigned int id;
	unsigned int l2_supported;
	unsigned int proto_supported;

        int (*reset)(struct rfid_reader_handle *h);

	/* open the reader */
	struct rfid_reader_handle * (*open)(void *data);

	/* Initialize the reader for a given layer 2 */
	int (*init)(struct rfid_reader_handle *h, enum rfid_layer2_id);

	/* completely close the reader */
	void (*close)(struct rfid_reader_handle *h);


	int (*getopt)(struct rfid_reader_handle *rh, int optname,
		      void *optval, unsigned int *optlen);

	int (*setopt)(struct rfid_reader_handle *rh, int optname,
		      const void *optval, unsigned int optlen);

	/* transceive one frame */
	int (*transceive)(struct rfid_reader_handle *h,
			  enum rfid_frametype frametype,
			  const unsigned char *tx_buf, unsigned int tx_len,
			  unsigned char *rx_buf, unsigned int *rx_len,
			  u_int64_t timeout, unsigned int flags);

	struct rfid_14443a_reader {
		int (*transceive_sf)(struct rfid_reader_handle *h,
				     unsigned char cmd,
				     struct iso14443a_atqa *atqa);
		int (*transceive_acf)(struct rfid_reader_handle *h,
				      struct iso14443a_anticol_cmd *cmd,
				      unsigned int *bit_of_col);
		int (*set_speed)(struct rfid_reader_handle *h,
				 unsigned int tx,
				 unsigned int speed);
		unsigned int speed;
	} iso14443a;
	struct rfid_14443b_reader {
		unsigned int speed;
	} iso14443b;
	struct rfid_15693_reader {
		int (*transceive_ac)(struct rfid_reader_handle *h,
				     const struct iso15693_anticol_cmd *acf,
				     unsigned int acf_len,
				     struct iso15693_anticol_resp *resp,
				     unsigned int *rx_len, char *bit_of_col);
	} iso15693;
	struct rfid_mifare_classic_reader {
		int (*setkey)(struct rfid_reader_handle *h, const unsigned char *key);
		int (*setkey_ee)(struct rfid_reader_handle *h, const unsigned int addr);
		int (*auth)(struct rfid_reader_handle *h, u_int8_t cmd,
			    u_int32_t serno, u_int8_t block);
	} mifare_classic;
};

enum rfid_reader_id {
	RFID_READER_CM5121,
	RFID_READER_PEGODA,
	RFID_READER_OPENPCD,
	RFID_READER_SPIDEV,
};

struct rfid_reader_handle {
	struct rfid_asic_handle *ah;

	union {

	} priv;
	const struct rfid_reader *reader;
};

extern struct rfid_reader_handle *
rfid_reader_open(void *data, unsigned int id);

extern void rfid_reader_close(struct rfid_reader_handle *rh);

extern int rfid_reader_getopt(struct rfid_reader_handle *rh, int optname,
			      void *optval, unsigned int *optlen);
extern int rfid_reader_setopt(struct rfid_reader_handle *rh, int optname,
			      const void *optval, unsigned int optlen);

#endif
