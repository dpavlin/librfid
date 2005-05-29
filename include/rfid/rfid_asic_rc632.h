#ifndef _RFID_ASIC_RC632_H
#define _RFID_ASIC_RC632_H

struct rfid_asic_transport_handle;

struct rfid_asic_rc632_transport {
	struct {
		int (*reg_write)(struct rfid_asic_transport_handle *rath,
				 unsigned char reg,
				 unsigned char value);
		int (*reg_read)(struct rfid_asic_transport_handle *rath,
				unsigned char reg,
				unsigned char *value);
		int (*fifo_write)(struct rfid_asic_transport_handle *rath,
				  unsigned char len,
				  const unsigned char *buf,
				  unsigned char flags);
		int (*fifo_read)(struct rfid_asic_transport_handle *rath,
				 unsigned char len,
				 unsigned char *buf);
	} fn;
};

struct rfid_asic_handle;

struct iso14443a_atqua;
struct iso14443a_anticol_cmd;

struct rfid_asic_rc632 {
	struct {
		int (*power_up)(struct rfid_asic_handle *h);
		int (*power_down)(struct rfid_asic_handle *h);
		int (*turn_on_rf)(struct rfid_asic_handle *h);
		int (*turn_off_rf)(struct rfid_asic_handle *h);
		int (*transcieve)(struct rfid_asic_handle *h,
				  const unsigned char *tx_buf,
				  unsigned int tx_len,
				  unsigned char *rx_buf,
				  unsigned int *rx_len,
				  unsigned int timeout,
				  unsigned int flags);
		struct {
			int (*init)(struct rfid_asic_handle *h);
			int (*transcieve_sf)(struct rfid_asic_handle *h,
					     unsigned char cmd,
					     struct iso14443a_atqa *atqa);
			int (*transcieve_acf)(struct rfid_asic_handle *h,
					      struct iso14443a_anticol_cmd *cmd,
					      unsigned int *bit_of_col);
		} iso14443a;
		struct {
			int (*init)(struct rfid_asic_handle *h);
		} iso14443b;
	} fn;
};

struct rc632_transport_handle {
};

/* A handle to a specific RC632 chip */
struct rfid_asic_rc632_handle {
	struct rc632_transport_handle th;
};

int 
rc632_reg_write(struct rfid_asic_handle *handle,
		unsigned char reg,
		unsigned char val);

int 
rc632_reg_read(struct rfid_asic_handle *handle,
	       unsigned char reg,
	       unsigned char *val);
int 
rc632_fifo_write(struct rfid_asic_handle *handle,
		 unsigned char len,
		 const unsigned char *buf,
		 unsigned char flags);

int 
rc632_fifo_read(struct rfid_asic_handle *handle,
		unsigned char len,
		unsigned char *buf);

int
rc632_set_bits(struct rfid_asic_handle *handle, unsigned char reg,
		unsigned char val);

int 
rc632_clear_bits(struct rfid_asic_handle *handle, unsigned char reg,
		 unsigned char val);


int 
rc632_turn_on_rf(struct rfid_asic_handle *handle);


int 
rc632_turn_off_rf(struct rfid_asic_handle *handle);

int
rc632_power_up(struct rfid_asic_handle *handle);

int
rc632_power_down(struct rfid_asic_handle *handle);


int
rc632_wait_idle(struct rfid_asic_handle *handle, unsigned int time);

int
rc632_transmit(struct rfid_asic_handle *handle,
		const unsigned char *buf,
		unsigned char len,
		unsigned int timeout);

int
rc632_transcieve(struct rfid_asic_handle *handle,
		 const unsigned char *tx_buf,
		 unsigned char tx_len,
		 unsigned char *rx_buf,
		 unsigned char *rx_len,
		 unsigned int timer,
		 unsigned int toggle);

int
rc632_read_eeprom(struct rfid_asic_handle *handle);


int
rc632_calc_crc16_from(struct rfid_asic_handle *handle);

int
rc632_register_dump(struct rfid_asic_handle *handle, unsigned char *buf);


//struct rfid_asic_handle * rc632_open(struct rc632_transport *transport, void *data);


extern struct rfid_asic rc632;
#endif
