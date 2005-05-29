/* 
 * Philips CL RC632 primitives for ISO 14443-A compliant PICC's
 *
 * (C) 2005 by Harald Welte <laforge@gnumonks.org>
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <rfid/rfid.h>
#include <rfid/rfid_asic_rc632.h>
#include <rfid/rfid_layer2_iso14443a.h>

#include "rc632.h"

#include "cm5121_rfid.h"	/* FIXME: this needs to be modular */

static int
rc632_iso14443a_init(struct rfid_asic_handle *handle)
{
	int ret;

#if 0
	ret = rc632_power_up(handle);
	if (ret < 0)
		return ret;

	ret = rc632_turn_on_rf(handle);
	if (ret < 0)
		return ret;
#endif

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

	ret = rc632_reg_write(handle, RC632_REG_RX_WAIT, 0x03);
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

#if 0
int
rc632_iso14443a_select(struct rc632_handle *handle,
		unsigned char *retptr,
	)
{
	int ret;
	unsigned char tx_buf[7];
	unsigned char rx_buf[64];
	unsigned char rx_len = 1;

	memset(rx_buf, 0, sizeof(rx_buf));

	tx_buf[0] = arg_8;
	tx_buf[1] = 0x70;
	(u_int32_t *)tx_buf[2] = arg_4;
	tx_buf[6] = arg4+4;

	/* disable mifare cryto */
	ret = rc632_clear_bit(handle, RC632_REG_CONTROL,
				RC632_CONTROL_CRYPTO1_ON);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_CHANNEL_REDUNDANCY,
				(RC632_CR_PARITY_ENABLE |
				 RC632_CR_PARITY_ODD |
				 RC632_CR_TX_CRC_ENABLE |
				 RC632_CR_RX_CRC_ENABLE));
	if (ret < 0)
		return ret;

	ret = rc632_transcieve(handle, tx_buf, sizeof(tx_buf),
				rx_buf, &rx_len, 0x32, 0);

	if (ret < 0 || rx_len != 1)
		return ret;

	*retptr = rx_buf[0];

	return 0;
}

/* issue a 14443-3 A PCD -> PICC command, such as REQA, WUPA */
int
rc632_iso14443a_req(sutruct rc632_handle *handle, unsigned char req,
		    unsigned char *resp)
{
	int ret;
	unsigned char tx_buf[1];
	unsigned char rx_buf[0x40];
	unsigned char rx_len = 2;

	memset(rx_buf, 0, sizeof(rx_buf));

	tx_buf[0] = req;

	/* transfer only 7 bits of last byte in frame */
	ret = rc632_reg_write(handle, RC632_REG_BIT_FRAMING, 0x07);
	if (ret < 0)
		return ret;

	
	ret = rc632_clear_bits(handle, RC632_REG_CONTROL,
				RC632_CONTROL_CRYPTO1_ON);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_CHANNEL_REDUNDANCY,
				(RC632_CR_PARITY_ENABLE |
				 RC632_CR_PARITY_ODD));
	if (ret < 0)
		return ret;

	ret = rc632_transcieve(handle, tx_buf, sizeof(tx_buf), rx_buf,
				&rx_len, 0x32, 0);
	if (ret < 0)
		return ret;

	/* switch back to normal 8bit last byte */
	ret = rc632_reg_write(handle, RC632_REG_BIT_FRAMING, 0x00);
	if (ret < 0)
		return ret;

	if ((rx_len != 2) || (rx_buf[1] != 0xf0))
		return -1;

	resp[0] = rx_buf[0];
	resp[1] = rx_buf[1];
	
	return 0;
}
#endif

/* issue a 14443-3 A PCD -> PICC command in a short frame, such as REQA, WUPA */
static int
rc632_iso14443a_transcieve_sf(struct rfid_asic_handle *handle,
				unsigned char cmd,
		    		struct iso14443a_atqa *atqa)
{
	int ret;
	unsigned char tx_buf[1];
	unsigned char rx_len = 2;

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

	ret = rc632_reg_write(handle, RC632_REG_CHANNEL_REDUNDANCY,
				(RC632_CR_PARITY_ENABLE |
				 RC632_CR_PARITY_ODD));
	if (ret < 0)
		return ret;

	ret = rc632_transcieve(handle, tx_buf, sizeof(tx_buf),
				(unsigned char *)atqa, &rx_len, 0x32, 0);
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

/* trasncieve regular frame */
static int
rc632_iso14443a_transcieve(struct rfid_asic_handle *handle,
			   const unsigned char *tx_buf, unsigned int tx_len,
			   unsigned char *rx_buf, unsigned int *rx_len,
			   unsigned int timeout, unsigned int flags)
{
	int ret;
	unsigned char rxl = *rx_len & 0xff;

	memset(rx_buf, 0, *rx_len);

	ret = rc632_reg_write(handle, RC632_REG_CHANNEL_REDUNDANCY,
				(RC632_CR_PARITY_ENABLE |
				 RC632_CR_PARITY_ODD |
				 RC632_CR_TX_CRC_ENABLE |
				 RC632_CR_RX_CRC_ENABLE));
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
	unsigned char rx_buf[64];
	unsigned char rx_len = sizeof(rx_buf);
	unsigned char rx_align = 0, tx_last_bits, tx_bytes;
	unsigned char boc;
	unsigned char error_flag;
	*bit_of_col = ISO14443A_BITOFCOL_NONE;
	memset(rx_buf, 0, sizeof(rx_buf));

	/* disable mifare cryto */
	ret = rc632_clear_bits(handle, RC632_REG_CONTROL,
				RC632_CONTROL_CRYPTO1_ON);
	if (ret < 0)
		return ret;

	/* disalbe CRC summing */
	ret = rc632_reg_write(handle, RC632_REG_CHANNEL_REDUNDANCY,
				(RC632_CR_PARITY_ENABLE |
				 RC632_CR_PARITY_ODD));
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

	ret = rc632_transcieve(handle, (unsigned char *)acf, tx_bytes,
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


