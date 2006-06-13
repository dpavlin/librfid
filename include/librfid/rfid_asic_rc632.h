#ifndef _RFID_ASIC_RC632_H
#define _RFID_ASIC_RC632_H

struct rfid_asic_transport_handle;

#include <sys/types.h>
#include <librfid/rfid_asic.h>

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

struct rfid_asic_rc632 {
	struct {
		int (*power_up)(struct rfid_asic_handle *h);
		int (*power_down)(struct rfid_asic_handle *h);
		int (*turn_on_rf)(struct rfid_asic_handle *h);
		int (*turn_off_rf)(struct rfid_asic_handle *h);
		int (*transceive)(struct rfid_asic_handle *h,
				  enum rfid_frametype,
				  const u_int32_t *tx_buf,
				  unsigned int tx_len,
				  u_int32_t *rx_buf,
				  unsigned int *rx_len,
				  u_int64_t timeout,
				  unsigned int flags);
		struct {
			int (*init)(struct rfid_asic_handle *h);
			int (*transceive_sf)(struct rfid_asic_handle *h,
					     u_int32_t cmd,
					     struct iso14443a_atqa *atqa);
			int (*transceive_acf)(struct rfid_asic_handle *h,
					      struct iso14443a_anticol_cmd *cmd,
					      unsigned int *bit_of_col);
			int (*set_speed)(struct rfid_asic_handle *h,
					 unsigned int tx,
					 unsigned int speed);
		} iso14443a;
		struct {
			int (*init)(struct rfid_asic_handle *h);
		} iso14443b;
		struct {
			int (*init)(struct rfid_asic_handle *h);
		} iso15693;
		struct {
			int (*setkey)(struct rfid_asic_handle *h,
				      const unsigned char *key);
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

#if 0
int 
rc632_reg_write(struct rfid_asic_handle *handle,
		u_int8_t reg,
		u_int8_t val);

int 
rc632_reg_read(struct rfid_asic_handle *handle,
	       u_int8_t reg,
	       u_int8_t *val);
int 
rc632_fifo_write(struct rfid_asic_handle *handle,
		 u_int8_t len,
		 const u_int32_t *buf,
		 u_int8_t flags);

int 
rc632_fifo_read(struct rfid_asic_handle *handle,
		u_int8_t len,
		u_int8_t *buf);

int
rc632_set_bits(struct rfid_asic_handle *handle, u_int8_t reg,
		u_int82_t val);

int 
rc632_clear_bits(struct rfid_asic_handle *handle, u_int32_t reg,
		 u_int32_t val);


int 
rc632_turn_on_rf(struct rfid_asic_handle *handle);


int 
rc632_turn_off_rf(struct rfid_asic_handle *handle);

int
rc632_power_up(struct rfid_asic_handle *handle);

int
rc632_power_down(struct rfid_asic_handle *handle);


int
rc632_wait_idle(struct rfid_asic_handle *handle, u_int64_t time);

int
rc632_transmit(struct rfid_asic_handle *handle,
		const u_int32_t *buf,
		u_int32_t len,
		u_int64_t timeout);

int
rc632_transceive(struct rfid_asic_handle *handle,
		 const u_int32_t *tx_buf,
		 u_int32_t tx_len,
		 u_int32_t *rx_buf,
		 u_int32_t *rx_len,
		 unsigned int timer,
		 unsigned int toggle);

int
rc632_read_eeprom(struct rfid_asic_handle *handle);


int
rc632_calc_crc16_from(struct rfid_asic_handle *handle);

int
rc632_register_dump(struct rfid_asic_handle *handle, u_int32_t *buf);


struct rfid_asic_handle * rc632_open(struct rfid_asic_transport_handle *th);


extern struct rfid_asic rc632;
#endif

#endif
