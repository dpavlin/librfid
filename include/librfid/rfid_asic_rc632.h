#ifndef _RFID_ASIC_RC632_H
#define _RFID_ASIC_RC632_H

struct rfid_asic_transport_handle;

#include <librfid/rfid.h>
#include <librfid/rfid_asic.h>
#include <librfid/rfid_layer2.h>

struct rfid_asic_rc632_transport {
	struct {
		int (*reg_write)(struct rfid_asic_transport_handle *rath,
				 u_int8_t reg,
				 u_int8_t value);
		int (*reg_read)(struct rfid_asic_transport_handle *rath,
				u_int8_t reg,
				u_int8_t *value);
		int (*fifo_write)(struct rfid_asic_transport_handle *rath,
				  u_int8_t len,
				  const u_int8_t *buf,
				  u_int8_t flags);
		int (*fifo_read)(struct rfid_asic_transport_handle *rath,
				 u_int8_t len,
				 u_int8_t *buf);
	} fn;
};

struct rfid_asic_handle;

struct iso14443a_atqa;
struct iso14443a_anticol_cmd;
struct iso15693_anticol_cmd;
struct iso15693_anticol_resp;

struct rfid_asic_rc632 {
	struct {
		int (*power)(struct rfid_asic_handle *h, int on);
		int (*rf_power)(struct rfid_asic_handle *h, int on);
		int (*init)(struct rfid_asic_handle *h, enum rfid_layer2_id);
		int (*transceive)(struct rfid_asic_handle *h,
				  enum rfid_frametype,
				  const u_int8_t *tx_buf,
				  unsigned int tx_len,
				  u_int8_t *rx_buf,
				  unsigned int *rx_len,
				  u_int64_t timeout,
				  unsigned int flags);
		struct {
			int (*transceive_sf)(struct rfid_asic_handle *h,
					     u_int8_t cmd,
					     struct iso14443a_atqa *atqa);
			int (*transceive_acf)(struct rfid_asic_handle *h,
					      struct iso14443a_anticol_cmd *cmd,
					      unsigned int *bit_of_col);
			int (*set_speed)(struct rfid_asic_handle *h,
					 unsigned int tx,
					 unsigned int speed);
		} iso14443a;
		struct {
			int (*transceive_ac)(struct rfid_asic_handle *h,
					     const struct iso15693_anticol_cmd *acf,
					     unsigned int acf_len,
					     struct iso15693_anticol_resp *resp,
					     unsigned int *rx_len, unsigned char *bit_of_col);
		} iso15693;
		struct {
			int (*setkey)(struct rfid_asic_handle *h,
				      const unsigned char *key);
			int (*setkey_ee)(struct rfid_asic_handle *h,
				      const unsigned int addr);
			int (*auth)(struct rfid_asic_handle *h, u_int8_t cmd, 
				    u_int32_t serno, u_int8_t block);
		} mifare_classic;
	} fn;
};

struct rc632_transport_handle {
};

/* A handle to a specific RC632 chip */
struct rfid_asic_rc632_handle {
	struct rc632_transport_handle th;
};

struct rfid_asic_rc632_impl_proto {
	u_int8_t mod_conductance;
	u_int8_t cw_conductance;
	u_int8_t bitphase;
	u_int8_t threshold;
};

struct rfid_asic_rc632_impl {
	u_int32_t mru;		/* maximum receive unit (PICC->PCD) */
	u_int32_t mtu;		/* maximum transmit unit (PCD->PICC) */
	//struct rfid_asic_rc632_impl_proto proto[NUM_RFID_PROTOCOLS];
};

extern struct rfid_asic_handle * rc632_open(struct rfid_asic_transport_handle *th);
extern void rc632_close(struct rfid_asic_handle *h);
extern int rc632_register_dump(struct rfid_asic_handle *handle, u_int8_t *buf);


/* register decoding inlines... */
#define DEBUGP_ERROR_FLAG(value) do {DEBUGP("error_flag: 0x%0.2x",value); \
				if (value & RC632_ERR_FLAG_CRC_ERR ) \
					DEBUGPC(", CRC"); \
				if (value & RC632_ERR_FLAG_COL_ERR ) \
					DEBUGPC(", COL"); \
				if (value & RC632_ERR_FLAG_FRAMING_ERR ) \
					DEBUGPC(", FRAMING"); \
				if (value & RC632_ERR_FLAG_PARITY_ERR) \
					DEBUGPC(", PARITY"); \
				if (value & RC632_ERR_FLAG_KEY_ERR ) \
					DEBUGPC(", KEY"); \
				if (value & RC632_ERR_FLAG_ACCESS_ERR ) \
					DEBUGPC(", ACCESS"); \
                DEBUGPC("\n");} while (0);

#define DEBUGP_STATUS_FLAG(foo) do {\
			DEBUGP("status_flag: 0x%0.2x",foo); \
			if (foo & RC632_STAT_ERR ) \
				DEBUGPC(", ERR"); \
			if (foo & RC632_STAT_HIALERT ) \
				DEBUGPC(", Hi"); \
			if (foo & RC632_STAT_IRQ ) \
				DEBUGPC(", IRQ"); \
			if (foo & RC632_STAT_LOALERT )  \
				DEBUGPC(", Lo"); \
			if ((foo & RC632_STAT_MODEM_MASK) == RC632_STAT_MODEM_AWAITINGRX )  \
				DEBUGPC(", mAwaitingRX"); \
			if ((foo & RC632_STAT_MODEM_MASK) == RC632_STAT_MODEM_GOTORX )  \
				DEBUGPC(", mGotoRX"); \
			if ((foo & RC632_STAT_MODEM_MASK) == RC632_STAT_MODEM_IDLE )  \
				DEBUGPC(", mIdle"); \
			if ((foo & RC632_STAT_MODEM_MASK) == RC632_STAT_MODEM_PREPARERX )  \
				DEBUGPC(", mPrepareRX"); \
			if ((foo & RC632_STAT_MODEM_MASK) == RC632_STAT_MODEM_RECV )  \
				DEBUGPC(", mRX"); \
			if ((foo & RC632_STAT_MODEM_MASK) == RC632_STAT_MODEM_TXDATA )  \
				DEBUGPC(", mTXData"); \
			if ((foo & RC632_STAT_MODEM_MASK) == RC632_STAT_MODEM_TXEOF )  \
				DEBUGPC(", mTXeof"); \
			if ((foo & RC632_STAT_MODEM_MASK) == RC632_STAT_MODEM_TXSOF )  \
				DEBUGPC(", mTXsof"); \
            DEBUGPC("\n"); } while (0);

#define DEBUGP_INTERRUPT_FLAG(txt,foo) do {\
                DEBUGP("%s: 0x%0.2x",txt,foo); \
                if (foo & RC632_INT_HIALERT) \
                    DEBUGPC(", HiA"); \
                if (foo & RC632_INT_LOALERT) \
                    DEBUGPC(", LoA"); \
                if (foo & RC632_INT_IDLE) \
                    DEBUGPC(", IDLE"); \
                if (foo & RC632_INT_RX) \
                    DEBUGPC(", RX"); \
                if (foo & RC632_INT_TX) \
                    DEBUGPC(", TX"); \
                if (foo & RC632_INT_TIMER) \
                    DEBUGPC(", TIMER"); \
                if (foo & RC632_INT_SET) \
                    DEBUGPC(", SET"); \
                DEBUGPC("\n"); } while (0);

#endif
