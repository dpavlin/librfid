#ifndef _RDR_RC632_COMMON
#define _RDR_RC632_COMMON

int _rdr_rc632_transceive(struct rfid_reader_handle *rh,
			  enum rfid_frametype frametype,
			  const unsigned char *tx_data, unsigned int tx_len,
			  unsigned char *rx_data, unsigned int *rx_len,
			  u_int64_t timeout, unsigned int flags);
int _rdr_rc632_transceive_sf(struct rfid_reader_handle *rh,
			     unsigned char cmd, struct iso14443a_atqa *atqa);
int _rdr_rc632_transceive_acf(struct rfid_reader_handle *rh,
			      struct iso14443a_anticol_cmd *cmd,
			      unsigned int *bit_of_col);
int _rdr_rc632_iso15693_transceive_ac(struct rfid_reader_handle *rh,
				      const struct iso15693_anticol_cmd *acf,
				      unsigned int acf_len,
				      struct iso15693_anticol_resp *resp,
				      unsigned int *resp_len, char *bit_of_col);
int _rdr_rc632_14443a_set_speed(struct rfid_reader_handle *rh, unsigned int tx,
				unsigned int speed);
int _rdr_rc632_l2_init(struct rfid_reader_handle *rh, enum rfid_layer2_id l2);
int _rdr_rc632_mifare_setkey(struct rfid_reader_handle *rh, const u_int8_t *key);
int _rdr_rc632_mifare_setkey_ee(struct rfid_reader_handle *rh, const unsigned int addr);
int _rdr_rc632_mifare_auth(struct rfid_reader_handle *rh, u_int8_t cmd, 
			   u_int32_t serno, u_int8_t block);
int _rdr_rc632_getopt(struct rfid_reader_handle *rh, int optname,
		      void *optval, unsigned int *optlen);
int _rdr_rc632_setopt(struct rfid_reader_handle *rh, int optname,
		      const void *optval, unsigned int optlen);

#endif
