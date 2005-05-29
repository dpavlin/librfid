
#include "rc632.h"

int rc632_iso14443b_init(struct rc632_handle *handle)
{
	// FIXME: some FIFO work
	//
	ret = rc632_reg_write(handle, RC632_REG_TX_CONTROL,
			      (RC632_TXCTRL_TX1_RF_EN |
			       RC632_TXCTRL_TX2_RF_EN |
			       RC632_TXCTRL_TX2_INV |
			       RC632_TXCTRL_MOD_SRC_INT));

	ret = rc632_reg_write(handle, RC632_REG_CW_CONDUCTANCE, 0x3f);

	ret = rc632_reg_write(handle, RC632_REG_MOD_CONDUCTANCE, 0x04);

	ret = rc632_reg_write(handle, RC632_REG_CODE_CONTROL,
			      (RC632_CDRCTRL_TXCD_NRZ |
			       RC632_CDRCTRL_RATE_14443B));

	ret = rc632_reg_write(handle, RC632_REG_MOD_WIDTH, 0x13);

	ret = rc632_reg_write(handle, RC632_REG_SOF_WIDTH, 0x3f);

	ret = rc632_reg_write(handle, RC632_REG_TYPE_B_FRAMING,
			      (RC632_TBFRAMING_SOF_11L_3H |
			       (6 << RC632_TBFRAMING_SPACE_SHIFT) |
			       RC632_TBFRAMING_EOF_11));

	ret = rc632_reg_write(handle, RC632_REG_RX_CONTROL1,
			      (RC632_RXCTRL1_GAIN_35DB |
			       RC632_RXCTRL1_ISO14443 |
			       RC632_RXCTRL1_SUBCP_8));

	ret = rc632_reg_write(handle, RC632_REG_DECODE_CONTROL,
			      (RC632_DECCTRL_BPSK |
			       RC632_DECCTRL_RXFR_14443B));

	ret = rc632_reg_write(handle, RC632_REG_BIT_PHASE,
				CM5121_14443B_BITPHASE);

	ret = rc632_reg_write(handle, RC632_REG_RX_THRESHOLD,
				CM5121_14443B_THRESHOLD);

	ret = rc632_reg_write(handle, RC632_REG_BPSK_DEM_CONTROL,
			      ((0x10 & RC632_BPSKD_TAUB_MASK)<<RC632_BPSKD_TAUB_SHIFT |
			       (0x11 & RC632_BPSKD_TAUD_MASK)<<RC632_BPSKD_TAUD_SHIFT |
			       RC632_BPSKD_FILTER_AMP_DETECT |
			       RC632_BPSKD_NO_RX_EOF |
			       RC632_BPSKD_NO_RX_EGT));

	ret = rc632_reg_write(handle, RC632_REG_RX_CONTROL2,
			      (RC632_RXCTRL2_AUTO_PD |
			       RC632_RXCTRL2_DECSRC_INT));

	ret = rc632_reg_write(handle, RC632_REG_RX_WAIT, 0x03);

	ret = rc632_reg_write(handle, RC632_REG_CHANNEL_RDUNDANCY,
			      (RC632_CR_TX_CRC_ENABLE |
			       RC632_CR_RX_CRC_ENABLE |
			       RC632_CR_CR3309));

	ret = rc632_reg_write(handle, RC632_REG_CRC_PRESET_LSB, 0xff);

	ret = rc632_reg_write(handle, RC632_REG_CRC_PRESET_MSB, 0xff);

}

