/* Generic Philips CL RC632 Routines
 *
 * (C) Harald Welte <laforge@gnumonks.org>
 *
 */

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 
 *  as published by the Free Software Foundation
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>

#include <librfid/rfid.h>
#include <librfid/rfid_asic.h>
#include <librfid/rfid_asic_rc632.h>
#include <librfid/rfid_reader_cm5121.h>
#include <librfid/rfid_layer2_iso14443a.h>
#include <librfid/rfid_protocol_mifare_classic.h>

#include "rfid_iso14443_common.h"
#include "rc632.h"
//#include "rc632_14443a.h"


#define RC632_TMO_AUTH1	14000

#define ENTER()		DEBUGP("entering\n")
struct rfid_asic rc632;

/* Register and FIFO Access functions */
static int 
rc632_reg_write(struct rfid_asic_handle *handle,
		u_int8_t reg,
		u_int8_t val)
{
	return handle->rath->rat->priv.rc632.fn.reg_write(handle->rath, reg, val);
}

static int 
rc632_reg_read(struct rfid_asic_handle *handle,
	       u_int8_t reg,
	       u_int8_t *val)
{
	return handle->rath->rat->priv.rc632.fn.reg_read(handle->rath, reg, val);
}

static int 
rc632_fifo_write(struct rfid_asic_handle *handle,
		 u_int8_t len,
		 const u_int8_t *buf,
		 u_int8_t flags)
{
	return handle->rath->rat->priv.rc632.fn.fifo_write(handle->rath, 
							   len, buf, flags);
}

static int 
rc632_fifo_read(struct rfid_asic_handle *handle,
		u_int8_t len,
		u_int8_t *buf)
{
	return handle->rath->rat->priv.rc632.fn.fifo_read(handle->rath, len, buf);
}


static int
rc632_set_bits(struct rfid_asic_handle *handle, 
		u_int8_t reg,
		u_int8_t val)
{
	int ret;
	u_int8_t tmp;

	ret = rc632_reg_read(handle, reg, &tmp);
	if (ret < 0)
		return -1;

	/* if bits are already set, no need to set them again */
	if ((tmp & val) == val)
		return 0;

	return rc632_reg_write(handle, reg, (tmp|val)&0xff);
}
static int 
rc632_set_bit_mask(struct rfid_asic_handle *handle, 
		   u_int8_t reg, u_int8_t mask, u_int8_t val)
{
	int ret;
	u_int8_t tmp;

	ret = rc632_reg_read(handle, reg, &tmp);
	if (ret < 0)
		return ret;

	/* if bits are already like we want them, abort */
	if ((tmp & mask) == val)
		return 0;

	return rc632_reg_write(handle, reg, (tmp & ~mask)|(val & mask));
}

static int 
rc632_clear_bits(struct rfid_asic_handle *handle, 
		 u_int8_t reg,
		 u_int8_t val)
{
	int ret;
	u_int8_t tmp;

	ret = rc632_reg_read(handle, reg, &tmp);
	if (ret < 0) {
		DEBUGP("error during reg_read(%p, %d):%d\n",
			handle, reg, ret);
		return -1;
	}
	/* if bits are already cleared, no need to clear them again */
	if ((tmp & val) == 0)
		return 0;

	return rc632_reg_write(handle, reg, (tmp & ~val)&0xff);
}

static int 
rc632_turn_on_rf(struct rfid_asic_handle *handle)
{
	ENTER();
	return rc632_set_bits(handle, RC632_REG_TX_CONTROL, 0x03);
}

static int 
rc632_turn_off_rf(struct rfid_asic_handle *handle)
{
	ENTER();
	return rc632_clear_bits(handle, RC632_REG_TX_CONTROL, 0x03);
}

static int
rc632_power_up(struct rfid_asic_handle *handle)
{
	ENTER();
	return rc632_clear_bits(handle, RC632_REG_CONTROL, 
				RC632_CONTROL_POWERDOWN);
}

static int
rc632_power_down(struct rfid_asic_handle *handle)
{
	return rc632_set_bits(handle, RC632_REG_CONTROL,
			      RC632_CONTROL_POWERDOWN);
}

/* Stupid RC623 implementations don't evaluate interrupts but poll the
 * command register for "status idle" */
static int
rc632_wait_idle(struct rfid_asic_handle *handle, u_int64_t timeout)
{
	u_int8_t cmd = 0xff;
	int ret;

	while (cmd != 0) {
		ret = rc632_reg_read(handle, RC632_REG_COMMAND, &cmd);
		if (ret < 0)
			return ret;

		if (cmd == 0) {
			/* FIXME: read second time ?? */
			return 0;
		}

		{
			u_int8_t foo;
			rc632_reg_read(handle, RC632_REG_PRIMARY_STATUS, &foo);
			if (foo & 0x04)
				rc632_reg_read(handle, RC632_REG_ERROR_FLAG, &foo);
		}

		usleep(100);

		/* Fixme: Abort after some timeout */
	}

	return 0;
}

static int
rc632_transmit(struct rfid_asic_handle *handle,
		const u_int8_t *buf,
		u_int8_t len,
		u_int64_t timeout)
{
	int ret;

	ret = rc632_fifo_write(handle, len, buf, 0x03);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_COMMAND, RC632_CMD_TRANSMIT);
	if (ret < 0)
		return ret;

	return rc632_wait_idle(handle, timeout);
}

static int
tcl_toggle_pcb(struct rfid_asic_handle *handle)
{
	// FIXME: toggle something between 0x0a and 0x0b
	return 0;
}

static int
rc632_transcieve(struct rfid_asic_handle *handle,
		 const u_int8_t *tx_buf,
		 u_int8_t tx_len,
		 u_int8_t *rx_buf,
		 u_int8_t *rx_len,
		 unsigned int timer,
		 unsigned int toggle)
{
	int ret;

	ret = rc632_fifo_write(handle, tx_len, tx_buf, 0x03);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_COMMAND, RC632_CMD_TRANSCIEVE);
	if (ret < 0)
		return ret;

	if (toggle == 1)
		tcl_toggle_pcb(handle);

	ret = rc632_wait_idle(handle, timer);
	if (ret < 0)
		return ret;

	ret = rc632_reg_read(handle, RC632_REG_FIFO_LENGTH, rx_len);
	if (ret < 0)
		return ret;

	if (*rx_len == 0) {
		u_int8_t tmp;

		DEBUGP("rx_len == 0\n");

		rc632_reg_read(handle, RC632_REG_ERROR_FLAG, &tmp);
		rc632_reg_read(handle, RC632_REG_CHANNEL_REDUNDANCY, &tmp);

		return -1; 
	}

	return rc632_fifo_read(handle, *rx_len, rx_buf);
}

static int
rc632_read_eeprom(struct rfid_asic_handle *handle)
{
	u_int8_t recvbuf[60];
	u_int8_t sndbuf[3];
	int ret;

	sndbuf[0] = 0x00;
	sndbuf[1] = 0x00;
	sndbuf[2] = 0x3c;

	ret = rc632_fifo_write(handle, 3, sndbuf, 0x03);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_COMMAND, RC632_CMD_READ_E2);
	if (ret < 0)
		return ret;

	usleep(20000);

	ret = rc632_fifo_read(handle, sizeof(recvbuf), recvbuf);
	if (ret < 0)
		return ret;

	// FIXME: do something with eeprom contents
	return ret;
}

static int
rc632_calc_crc16_from(struct rfid_asic_handle *handle)
{
	u_int8_t sndbuf[2] = { 0x01, 0x02 };
	u_int8_t crc_lsb = 0x00 , crc_msb = 0x00;
	int ret;

	ret = rc632_reg_write(handle, RC632_REG_CRC_PRESET_LSB, 0x12);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_CRC_PRESET_MSB, 0xe0);
	if (ret < 0)
		return ret;

	ret = rc632_fifo_write(handle, sizeof(sndbuf), sndbuf, 3);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_COMMAND, RC632_CMD_CALC_CRC);
	if (ret < 0)
		return ret;
	
	usleep(10000);	// FIXME: no checking for cmd completion?

	ret = rc632_reg_read(handle, RC632_REG_CRC_RESULT_LSB, &crc_lsb);
	if (ret < 0)
		return ret;

	ret = rc632_reg_read(handle, RC632_REG_CRC_RESULT_MSB, &crc_msb);
	if (ret < 0)
		return ret;

	// FIXME: what to do with crc result?
	return ret;
}


int
rc632_register_dump(struct rfid_asic_handle *handle, u_int8_t *buf)
{
	int ret;
	u_int8_t i;

	for (i = 0; i <= 0x3f; i++) {
		ret = rc632_reg_read(handle, i, &buf[i]);
		// do we want error checks?
	}
	return 0;
}



/* generic FIFO access functions (if no more efficient ones provided by
 * transport driver) */

static int 
generic_fifo_write()
{
	// FIXME: implementation (not needed for CM 5121)
	return -1;
}

static int
generic_fifo_read()
{
	// FIXME: implementation (not neded for CM 5121)
	return -1;
}

static int
rc632_init(struct rfid_asic_handle *ah)
{
	int ret;

	/* switch off rf (make sure PICCs are reset at init time) */
	ret = rc632_power_down(ah);
	if (ret < 0)
		return ret;

	usleep(10000);

	/* switch on rf */
	ret = rc632_power_up(ah);
	if (ret < 0)
		return ret;

	/* disable register paging */
	ret = rc632_reg_write(ah, 0x00, 0x00);
	if (ret < 0)
		return ret;

	/* set some sane default values */
	ret = rc632_reg_write(ah, 0x11, 0x5b);
	if (ret < 0)
		return ret;

	/* switch on rf */
	ret = rc632_turn_on_rf(ah);
	if (ret < 0)
		return ret;

	return 0;
}

static int
rc632_fini(struct rfid_asic_handle *ah)
{
	int ret;

	/* switch off rf */
	ret = rc632_turn_off_rf(ah);
	if (ret < 0)
		return ret;

	ret = rc632_power_down(ah);
	if (ret < 0)
		return ret;

	return 0;
}

struct rfid_asic_handle *
rc632_open(struct rfid_asic_transport_handle *th)
{
	struct rfid_asic_handle *h;

	h = malloc(sizeof(*h));
	if (!h)
		return NULL;
	memset(h, 0, sizeof(*h));

	h->asic = &rc632;
	h->rath = th;
	h->fc = h->asic->fc;
	h->mtu = h->mru = 40; /* FIXME */

	if (rc632_init(h) < 0) {
		free(h);
		return NULL;
	}

	return h;
}

void
rc632_close(struct rfid_asic_handle *h)
{
	rc632_fini(h);
	free(h);
}


/* 
 * Philips CL RC632 primitives for ISO 14443-A compliant PICC's
 *
 * (C) 2005 by Harald Welte <laforge@gnumonks.org>
 *
 */

static int
rc632_iso14443a_init(struct rfid_asic_handle *handle)
{
	int ret;

	// FIXME: some fifo work (drain fifo?)
	
	/* flush fifo (our way) */
	ret = rc632_reg_write(handle, RC632_REG_CONTROL, 0x01);

	ret = rc632_reg_write(handle, RC632_REG_TX_CONTROL,
			(RC632_TXCTRL_TX1_RF_EN |
			 RC632_TXCTRL_TX2_RF_EN |
			 RC632_TXCTRL_TX2_INV |
			 RC632_TXCTRL_FORCE_100_ASK |
			 RC632_TXCTRL_MOD_SRC_INT));
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_CW_CONDUCTANCE,
				CM5121_CW_CONDUCTANCE);
	if (ret < 0)
		return ret;

	/* Since FORCE_100_ASK is set (cf mc073930.pdf), this line may be left out? */
	ret = rc632_reg_write(handle, RC632_REG_MOD_CONDUCTANCE,
				CM5121_MOD_CONDUCTANCE);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_CODER_CONTROL,
				(RC632_CDRCTRL_TXCD_14443A |
				 RC632_CDRCTRL_RATE_106K));
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_MOD_WIDTH, 0x13);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_MOD_WIDTH_SOF, 0x3f);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_TYPE_B_FRAMING, 0x00);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_RX_CONTROL1,
			      (RC632_RXCTRL1_GAIN_35DB |
			       RC632_RXCTRL1_ISO14443 |
			       RC632_RXCTRL1_SUBCP_8));
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_DECODER_CONTROL,
			      (RC632_DECCTRL_MANCHESTER |
			       RC632_DECCTRL_RXFR_14443A));
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_BIT_PHASE,
				CM5121_14443A_BITPHASE);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_RX_THRESHOLD,
				CM5121_14443A_THRESHOLD);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_BPSK_DEM_CONTROL, 0x00);
	if (ret < 0)
		return ret;
			      
	ret = rc632_reg_write(handle, RC632_REG_RX_CONTROL2,
			      (RC632_RXCTRL2_DECSRC_INT |
			       RC632_RXCTRL2_CLK_Q));
	if (ret < 0)
		return ret;

	/* Omnikey proprietary driver has 0x03, but 0x06 is the default reset value ?!? */
	ret = rc632_reg_write(handle, RC632_REG_RX_WAIT, 0x06);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_CHANNEL_REDUNDANCY,
			      (RC632_CR_PARITY_ENABLE |
			       RC632_CR_PARITY_ODD));
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_CRC_PRESET_LSB, 0x63);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_CRC_PRESET_MSB, 0x63);
	if (ret < 0)
		return ret;

	return 0;
}

static int
rc632_iso14443a_fini(struct iso14443a_handle *handle_14443)
{

#if 0
	ret = rc632_turn_off_rf(handle);
	if (ret < 0)
		return ret;
#endif


	return 0;
}


/* issue a 14443-3 A PCD -> PICC command in a short frame, such as REQA, WUPA */
static int
rc632_iso14443a_transcieve_sf(struct rfid_asic_handle *handle,
				u_int8_t cmd,
		    		struct iso14443a_atqa *atqa)
{
	int ret;
	u_int8_t tx_buf[1];
	u_int8_t rx_len = 2;

	memset(atqa, 0, sizeof(atqa));

	tx_buf[0] = cmd;

	/* transfer only 7 bits of last byte in frame */
	ret = rc632_reg_write(handle, RC632_REG_BIT_FRAMING, 0x07);
	if (ret < 0)
		return ret;

	ret = rc632_clear_bits(handle, RC632_REG_CONTROL,
				RC632_CONTROL_CRYPTO1_ON);
	if (ret < 0)
		return ret;

#if 0
	ret = rc632_reg_write(handle, RC632_REG_CHANNEL_REDUNDANCY,
				(RC632_CR_PARITY_ENABLE |
				 RC632_CR_PARITY_ODD));
#else
	ret = rc632_clear_bits(handle, RC632_REG_CHANNEL_REDUNDANCY,
				RC632_CR_RX_CRC_ENABLE|RC632_CR_TX_CRC_ENABLE);
				
#endif
	if (ret < 0)
		return ret;

	ret = rc632_transcieve(handle, tx_buf, sizeof(tx_buf),
				(u_int8_t *)atqa, &rx_len, 0x32, 0);
	if (ret < 0) {
		DEBUGP("error during rc632_transcieve()\n");
		return ret;
	}

	/* switch back to normal 8bit last byte */
	ret = rc632_reg_write(handle, RC632_REG_BIT_FRAMING, 0x00);
	if (ret < 0)
		return ret;

	if (rx_len != 2) {
		DEBUGP("rx_len(%d) != 2\n", rx_len);
		return -1;
	}

	return 0;
}

/* transcieve regular frame */
static int
rc632_iso14443ab_transcieve(struct rfid_asic_handle *handle,
			   unsigned int frametype,
			   const u_int8_t *tx_buf, unsigned int tx_len,
			   u_int8_t *rx_buf, unsigned int *rx_len,
			   u_int64_t timeout, unsigned int flags)
{
	int ret;
	u_int8_t rxl = *rx_len & 0xff;
	u_int8_t channel_red;

	memset(rx_buf, 0, *rx_len);

	switch (frametype) {
	case RFID_14443A_FRAME_REGULAR:
	case RFID_MIFARE_FRAME:
		channel_red = RC632_CR_RX_CRC_ENABLE|RC632_CR_TX_CRC_ENABLE
				|RC632_CR_PARITY_ENABLE|RC632_CR_PARITY_ODD;
		break;
	case RFID_14443B_FRAME_REGULAR:
		channel_red = RC632_CR_RX_CRC_ENABLE|RC632_CR_TX_CRC_ENABLE
				|RC632_CR_CRC3309;
		break;
#if 0
	case RFID_MIFARE_FRAME:
		channel_red = RC632_CR_PARITY_ENABLE|RC632_CR_PARITY_ODD;
		break;
#endif
	default:
		return -EINVAL;
		break;
	}
	ret = rc632_reg_write(handle, RC632_REG_CHANNEL_REDUNDANCY,
			      channel_red);
	if (ret < 0)
		return ret;

	ret = rc632_transcieve(handle, tx_buf, tx_len, rx_buf, &rxl, 0x32, 0);
	*rx_len = rxl;
	if (ret < 0)
		return ret;


	return 0; 
}

/* transcieve anti collission bitframe */
static int
rc632_iso14443a_transcieve_acf(struct rfid_asic_handle *handle,
				struct iso14443a_anticol_cmd *acf,
				unsigned int *bit_of_col)
{
	int ret;
	u_int8_t rx_buf[64];
	u_int8_t rx_len = sizeof(rx_buf);
	u_int8_t rx_align = 0, tx_last_bits, tx_bytes;
	u_int8_t boc;
	u_int8_t error_flag;
	*bit_of_col = ISO14443A_BITOFCOL_NONE;
	memset(rx_buf, 0, sizeof(rx_buf));

	/* disable mifare cryto */
	ret = rc632_clear_bits(handle, RC632_REG_CONTROL,
				RC632_CONTROL_CRYPTO1_ON);
	if (ret < 0)
		return ret;

	/* disable CRC summing */
#if 0
	ret = rc632_reg_write(handle, RC632_REG_CHANNEL_REDUNDANCY,
				(RC632_CR_PARITY_ENABLE |
				 RC632_CR_PARITY_ODD));
#else
	ret = rc632_clear_bits(handle, RC632_REG_CHANNEL_REDUNDANCY,
				RC632_CR_TX_CRC_ENABLE|RC632_CR_TX_CRC_ENABLE);
#endif
	if (ret < 0)
		return ret;

	tx_last_bits = acf->nvb & 0x0f;	/* lower nibble indicates bits */
	tx_bytes = acf->nvb >> 4;
	if (tx_last_bits) {
		tx_bytes++;
		rx_align = (tx_last_bits+1) % 8;/* rx frame complements tx */
	}

	//rx_align = 8 - tx_last_bits;/* rx frame complements tx */

	/* set RxAlign and TxLastBits*/
	ret = rc632_reg_write(handle, RC632_REG_BIT_FRAMING,
				(rx_align << 4) | (tx_last_bits));
	if (ret < 0)
		return ret;

	ret = rc632_transcieve(handle, (u_int8_t *)acf, tx_bytes,
				rx_buf, &rx_len, 0x32, 0);
	if (ret < 0)
		return ret;

	/* bitwise-OR the two halves of the split byte */
	acf->uid_bits[tx_bytes-2] = (
		  (acf->uid_bits[tx_bytes-2] & (0xff >> (8-tx_last_bits)))
		| rx_buf[0]);
	/* copy the rest */
	memcpy(&acf->uid_bits[tx_bytes+1-2], &rx_buf[1], rx_len-1);

	/* determine whether there was a collission */
	ret = rc632_reg_read(handle, RC632_REG_ERROR_FLAG, &error_flag);
	if (ret < 0)
		return ret;

	if (error_flag & RC632_ERR_FLAG_COL_ERR) {
		/* retrieve bit of collission */
		ret = rc632_reg_read(handle, RC632_REG_COLL_POS, &boc);
		if (ret < 0)
			return ret;

		/* bit of collission relative to start of part 1 of 
		 * anticollision frame (!) */
		*bit_of_col = 2*8 + boc;
	}

	return 0;
}

enum rc632_rate {
	RC632_RATE_106	= 0x00,
	RC632_RATE_212	= 0x01,
	RC632_RATE_424	= 0x02,
	RC632_RATE_848	= 0x03,
};

struct rx_config {
	u_int8_t	subc_pulses;
	u_int8_t	rx_coding;
	u_int8_t	rx_threshold;
	u_int8_t	bpsk_dem_ctrl;
};

struct tx_config {
	u_int8_t	rate;
	u_int8_t	mod_width;
};

static struct rx_config rx_configs[] = {
	{
		.subc_pulses 	= RC632_RXCTRL1_SUBCP_8,
		.rx_coding	= RC632_DECCTRL_MANCHESTER,
		.rx_threshold	= 0x88,
		.bpsk_dem_ctrl	= 0x00,
	},
	{
		.subc_pulses	= RC632_RXCTRL1_SUBCP_4,
		.rx_coding	= RC632_DECCTRL_BPSK,
		.rx_threshold	= 0x50,
		.bpsk_dem_ctrl	= 0x0c,
	},
	{
		.subc_pulses	= RC632_RXCTRL1_SUBCP_2,
		.rx_coding	= RC632_DECCTRL_BPSK,
		.rx_threshold	= 0x50,
		.bpsk_dem_ctrl	= 0x0c,
	},
	{
		.subc_pulses	= RC632_RXCTRL1_SUBCP_1,
		.rx_coding	= RC632_DECCTRL_BPSK,
		.rx_threshold	= 0x50,
		.bpsk_dem_ctrl	= 0x0c,
	},
};

static struct tx_config tx_configs[] = {
	{
		.rate 		= RC632_CDRCTRL_RATE_106K,
		.mod_width	= 0x13,
	},
	{
		.rate		= RC632_CDRCTRL_RATE_212K,
		.mod_width	= 0x07,
	},
	{
		.rate		= RC632_CDRCTRL_RATE_424K,
		.mod_width	= 0x03,
	},
	{
		.rate		= RC632_CDRCTRL_RATE_848K,
		.mod_width	= 0x01,
	},
};

static int rc632_iso14443a_set_speed(struct rfid_asic_handle *handle,
				     unsigned int tx,
				     u_int8_t rate)
{
	int rc;
	u_int8_t reg;


	if (!tx) {
		/* Rx */
		if (rate > ARRAY_SIZE(rx_configs))
			return -EINVAL;

		rc = rc632_set_bit_mask(handle, RC632_REG_RX_CONTROL1,
					RC632_RXCTRL1_SUBCP_MASK,
					rx_configs[rate].subc_pulses);
		if (rc < 0)
			return rc;

		rc = rc632_set_bit_mask(handle, RC632_REG_DECODER_CONTROL,
					RC632_DECCTRL_BPSK,
					rx_configs[rate].rx_coding);
		if (rc < 0)
			return rc;

		rc = rc632_reg_write(handle, RC632_REG_RX_THRESHOLD,
					rx_configs[rate].rx_threshold);
		if (rc < 0)
			return rc;

		if (rx_configs[rate].rx_coding == RC632_DECCTRL_BPSK) {
			rc = rc632_reg_write(handle, 
					     RC632_REG_BPSK_DEM_CONTROL,
					     rx_configs[rate].bpsk_dem_ctrl);
			if (rc < 0)
				return rc;
		}
	} else {
		/* Tx */
		if (rate > ARRAY_SIZE(tx_configs))
			return -EINVAL;

		rc = rc632_set_bit_mask(handle, RC632_REG_CODER_CONTROL,
					RC632_CDRCTRL_RATE_MASK,
					tx_configs[rate].rate);
		if (rc < 0)
			return rc;

		rc = rc632_reg_write(handle, RC632_REG_MOD_WIDTH,
				     tx_configs[rate].mod_width);
		if (rc < 0)
			return rc;
	}

	return 0;
}

static int rc632_iso14443b_init(struct rfid_asic_handle *handle)
{
	int ret;

	// FIXME: some FIFO work
	
	/* flush fifo (our way) */
	ret = rc632_reg_write(handle, RC632_REG_CONTROL, 0x01);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_TX_CONTROL,
			(RC632_TXCTRL_TX1_RF_EN |
			 RC632_TXCTRL_TX2_RF_EN |
			 RC632_TXCTRL_TX2_INV |
			 RC632_TXCTRL_MOD_SRC_INT));
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_CW_CONDUCTANCE, 0x3f);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_MOD_CONDUCTANCE, 0x04);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_CODER_CONTROL,
			      (RC632_CDRCTRL_TXCD_NRZ |
			       RC632_CDRCTRL_RATE_14443B));
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_MOD_WIDTH, 0x13);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_MOD_WIDTH_SOF, 0x3f);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_TYPE_B_FRAMING,
			      (RC632_TBFRAMING_SOF_11L_3H |
			       (6 << RC632_TBFRAMING_SPACE_SHIFT) |
			       RC632_TBFRAMING_EOF_11));
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_RX_CONTROL1,
			      (RC632_RXCTRL1_GAIN_35DB |
			       RC632_RXCTRL1_ISO14443 |
			       RC632_RXCTRL1_SUBCP_8));
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_DECODER_CONTROL,
			      (RC632_DECCTRL_BPSK |
			       RC632_DECCTRL_RXFR_14443B));
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_BIT_PHASE,
				CM5121_14443B_BITPHASE);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_RX_THRESHOLD,
				CM5121_14443B_THRESHOLD);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_BPSK_DEM_CONTROL,
			      ((0x2 & RC632_BPSKD_TAUB_MASK)<<RC632_BPSKD_TAUB_SHIFT |
			       (0x3 & RC632_BPSKD_TAUD_MASK)<<RC632_BPSKD_TAUD_SHIFT |
			       RC632_BPSKD_FILTER_AMP_DETECT |
			       RC632_BPSKD_NO_RX_EOF |
			       RC632_BPSKD_NO_RX_EGT));
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_RX_CONTROL2,
			      (RC632_RXCTRL2_AUTO_PD |
			       RC632_RXCTRL2_DECSRC_INT));
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_RX_WAIT, 0x03);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_CHANNEL_REDUNDANCY,
			      (RC632_CR_TX_CRC_ENABLE |
			       RC632_CR_RX_CRC_ENABLE |
			       RC632_CR_CRC3309));
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_CRC_PRESET_LSB, 0xff);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_CRC_PRESET_MSB, 0xff);
	if (ret < 0)
		return ret;

	return 0;
}

static int
rc632_iso15693_init(struct rfid_asic_handle *h)
{
	int ret;

	ret = rc632_reg_write(h, RC632_REG_TX_CONTROL,
						(RC632_TXCTRL_MOD_SRC_INT |
						 RC632_TXCTRL_TX2_INV |
						 RC632_TXCTRL_TX2_RF_EN |
						 RC632_TXCTRL_TX1_RF_EN));
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_CW_CONDUCTANCE, 0x3f);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_MOD_CONDUCTANCE, 0x03);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_CODER_CONTROL,
						(RC632_CDRCTRL_RATE_15693 |
						 0x03)); /* FIXME */
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_MOD_WIDTH, 0x3f);
	if (ret < 0)
		return ret;
	
	ret = rc632_reg_write(h, RC632_REG_MOD_WIDTH_SOF, 0x3f);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_TYPE_B_FRAMING, 0x00);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_RX_CONTROL1, 
						(RC632_RXCTRL1_SUBCP_16 |
						 RC632_RXCTRL1_ISO15693 |
						 RC632_RXCTRL1_GAIN_35DB));
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_DECODER_CONTROL,
						(RC632_DECCTRL_RXFR_15693 |
						 RC632_DECCTRL_RX_INVERT));
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_BIT_PHASE, 0xe0);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_RX_THRESHOLD, 0xff);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_BPSK_DEM_CONTROL, 0x00);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_RX_CONTROL2,
						(RC632_RXCTRL2_AUTO_PD |
						 RC632_RXCTRL2_DECSRC_INT));
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_CHANNEL_REDUNDANCY,
						(RC632_CR_CRC3309 |
						 RC632_CR_RX_CRC_ENABLE |
						 RC632_CR_TX_CRC_ENABLE));
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_CRC_PRESET_LSB, 0xff);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_CRC_PRESET_MSB, 0xff);
	if (ret < 0)
		return ret;

	return 0;
}

static int
rc632_iso15693_icode_init(struct rfid_asic_handle *h)
{
	int ret;

	ret = rc632_reg_write(h, RC632_REG_TX_CONTROL,
						(RC632_TXCTRL_MOD_SRC_INT |
						 RC632_TXCTRL_TX2_INV |
						 RC632_TXCTRL_TX2_RF_EN |
						 RC632_TXCTRL_TX1_RF_EN));
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_CW_CONDUCTANCE, 0x3f);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_MOD_CONDUCTANCE, 0x02);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_CODER_CONTROL, 0x2c);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_MOD_WIDTH, 0x3f);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_MOD_WIDTH_SOF, 0x3f);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_MOD_WIDTH_SOF, 0x3f);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_TYPE_B_FRAMING, 0x00);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_RX_CONTROL1, 0x8b); /* FIXME */
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_DECODER_CONTROL, 0x00);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_BIT_PHASE, 0x52);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_RX_THRESHOLD, 0x66);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_BPSK_DEM_CONTROL, 0x00);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_RX_CONTROL2, 
						RC632_RXCTRL2_DECSRC_INT);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_CHANNEL_REDUNDANCY,
						(RC632_CR_RX_CRC_ENABLE |
						 RC632_CR_TX_CRC_ENABLE));
	ret = rc632_reg_write(h, RC632_REG_CRC_PRESET_LSB, 0xfe);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_CRC_PRESET_MSB, 0xff);
	if (ret < 0)
		return ret;

	return 0;
}

static int
rc632_iso15693_icl_init(struct rfid_asic_handle *h)
{
	int ret;
	
	/* ICL */

	ret = rc632_reg_write(h, RC632_REG_TX_CONTROL, 
						(RC632_TXCTRL_MOD_SRC_INT |	
						 RC632_TXCTRL_TX2_INV |
						 RC632_TXCTRL_TX2_RF_EN |
						 RC632_TXCTRL_TX1_RF_EN));
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_CW_CONDUCTANCE, 0x3f);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_MOD_CONDUCTANCE, 0x11);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_CODER_CONTROL, 
						(RC632_CDRCTRL_RATE_15693 |
						 RC632_CDRCTRL_TXCD_ICODE_STD |
						 0x03)); /* FIXME */
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_MOD_WIDTH, 0x3f);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_MOD_WIDTH_SOF, 0x3f);
	if (ret < 0)
		return ret;
	ret = rc632_reg_write(h, RC632_REG_RX_CONTROL1, 
						(RC632_RXCTRL1_SUBCP_16|
						 RC632_RXCTRL1_ISO15693|
						 RC632_RXCTRL1_GAIN_35DB));
	if (ret < 0)
		return ret;
	ret = rc632_reg_write(h, RC632_REG_DECODER_CONTROL,
						(RC632_DECCTRL_RX_INVERT|
						 RC632_DECCTRL_RXFR_15693));
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_BIT_PHASE, 0xbd);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_RX_THRESHOLD, 0xff);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_BPSK_DEM_CONTROL, 0x00);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_RX_CONTROL2, 
						RC632_RXCTRL2_DECSRC_INT);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_CHANNEL_REDUNDANCY, 0x00);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_CRC_PRESET_LSB, 0x12);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_CRC_PRESET_MSB, 0xe0);
	if (ret < 0)
		return ret;

	return 0;
}

struct mifare_authcmd {
	u_int8_t auth_cmd;
	u_int8_t block_address;
	u_int32_t serno;	/* lsb 1 2 msb */
} __attribute__ ((packed));


#define RFID_MIFARE_KEY_LEN 6
#define RFID_MIFARE_KEY_CODED_LEN 12

/* Transform crypto1 key from generic 6byte into rc632 specific 12byte */
static int
rc632_mifare_transform_key(const u_int8_t *key6, u_int8_t *key12)
{
	int i;
	u_int8_t ln;
	u_int8_t hn;

	for (i = 0; i < RFID_MIFARE_KEY_LEN; i++) {
		ln = key6[i] & 0x0f;
		hn = key6[i] >> 4;
		key12[i * 2 + 1] = (~ln << 4) | ln;
		key12[i * 2] = (~hn << 4) | hn;
	}
	return 0;
}

static int
rc632_mifare_set_key(struct rfid_asic_handle *h, const u_int8_t *key)
{
	u_int8_t coded_key[RFID_MIFARE_KEY_CODED_LEN];
	u_int8_t reg;
	int ret;

	ret = rc632_mifare_transform_key(key, coded_key);
	if (ret < 0)
		return ret;

	ret = rc632_fifo_write(h, RFID_MIFARE_KEY_CODED_LEN, coded_key, 0x03);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_COMMAND, RC632_CMD_LOAD_KEY);
	if (ret < 0)
		return ret;

	ret = rc632_wait_idle(h, RC632_TMO_AUTH1);
	if (ret < 0)
		return ret;

	ret = rc632_reg_read(h, RC632_REG_ERROR_FLAG, &reg);
	if (ret < 0)
		return ret;

	if (reg & RC632_ERR_FLAG_KEY_ERR)
		return -EINVAL;

	return 0;
}

static int
rc632_mifare_auth(struct rfid_asic_handle *h, u_int8_t cmd, u_int32_t serno,
		  u_int8_t block)
{
	int ret;
	struct mifare_authcmd acmd;
	u_int8_t reg;

	if (cmd != RFID_CMD_MIFARE_AUTH1A && cmd != RFID_CMD_MIFARE_AUTH1B)
		return -EINVAL;

	/* Initialize acmd */
	acmd.block_address = block & 0xff;
	acmd.auth_cmd = cmd;
	//acmd.serno = htonl(serno);
	acmd.serno = serno;

	ret = rc632_clear_bits(h, RC632_REG_CONTROL,
				RC632_CONTROL_CRYPTO1_ON);

	/* Clear Rx CRC */
	ret = rc632_clear_bits(h, RC632_REG_CHANNEL_REDUNDANCY,
				RC632_CR_RX_CRC_ENABLE);
	if (ret < 0)
		return ret;

	/* Send Authent1 Command */
	ret = rc632_fifo_write(h, sizeof(acmd), (unsigned char *)&acmd, 0x03);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_COMMAND, RC632_CMD_AUTHENT1);
	if (ret < 0)
		return ret;

	/* Wait until transmitter is idle */
	ret = rc632_wait_idle(h, RC632_TMO_AUTH1);
	if (ret < 0)
		return ret;

	ret = rc632_reg_read(h, RC632_REG_SECONDARY_STATUS, &reg);
	if (ret < 0)
		return ret;
	if (reg & 0x07)
		return -EIO;

	/* Clear Tx CRC */
	ret = rc632_clear_bits(h, RC632_REG_CHANNEL_REDUNDANCY,
				RC632_CR_TX_CRC_ENABLE);
	if (ret < 0)
		return ret;

	/* Send Authent2 Command */
	ret = rc632_reg_write(h, RC632_REG_COMMAND, RC632_CMD_AUTHENT2);
	if (ret < 0)
		return ret;

	/* Wait until transmitter is idle */
	ret = rc632_wait_idle(h, RC632_TMO_AUTH1);
	if (ret < 0)
		return ret;

	/* Check whether authentication was successful */
	ret = rc632_reg_read(h, RC632_REG_CONTROL, &reg);
	if (ret < 0)
		return ret;

	if (!(reg & RC632_CONTROL_CRYPTO1_ON))
		return -EACCES;

	return 0;

}

/* transcieve regular frame */
static int
rc632_mifare_transcieve(struct rfid_asic_handle *handle,
			const u_int8_t *tx_buf, unsigned int tx_len,
			u_int8_t *rx_buf, unsigned int *rx_len,
			u_int64_t timeout, unsigned int flags)
{
	int ret;
	u_int8_t rxl = *rx_len & 0xff;

	DEBUGP("entered\n");
	memset(rx_buf, 0, *rx_len);

#if 1
	ret = rc632_reg_write(handle, RC632_REG_CHANNEL_REDUNDANCY,
				(RC632_CR_PARITY_ENABLE |
				 RC632_CR_PARITY_ODD |
				 RC632_CR_TX_CRC_ENABLE |
				 RC632_CR_RX_CRC_ENABLE));
#else
	ret = rc632_clear_bits(handle, RC632_REG_CHANNEL_REDUNDANCY,
				RC632_CR_RX_CRC_ENABLE|RC632_CR_TX_CRC_ENABLE);
#endif
	if (ret < 0)
		return ret;

	ret = rc632_transcieve(handle, tx_buf, tx_len, rx_buf, &rxl, 0x32, 0);
	*rx_len = rxl;
	if (ret < 0)
		return ret;


	return 0; 
}

struct rfid_asic rc632 = {
	.name 	= "Philips CL RC632",
	.fc 	= ISO14443_FREQ_CARRIER,
	.priv.rc632 = {
		.fn = {
			.power_up = &rc632_power_up,
			.power_down = &rc632_power_down,
			.turn_on_rf = &rc632_turn_on_rf,
			.turn_off_rf = &rc632_turn_off_rf,
			.transcieve = &rc632_iso14443ab_transcieve,
			.iso14443a = {
				.init = &rc632_iso14443a_init,
				.transcieve_sf = &rc632_iso14443a_transcieve_sf,
				.transcieve_acf = &rc632_iso14443a_transcieve_acf,
				.set_speed = &rc632_iso14443a_set_speed,
			},
			.iso14443b = {
				.init = &rc632_iso14443b_init,
			},
			.iso15693 = {
				.init = &rc632_iso15693_init,
			},
			.mifare_classic = {
				.setkey = &rc632_mifare_set_key,
				.auth = &rc632_mifare_auth,
			},
		},
	},
};
