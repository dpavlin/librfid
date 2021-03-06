/* Register definitions for Philips CL RC632 RFID Reader IC
 *
 * (C) 2005 Harald Welte <laforge@gnumonks.org>
 *
 * Licensed under GNU General Public License, Version 2
 */

enum rc632_registers {
	RC632_REG_PAGE0			= 0x00,
	RC632_REG_COMMAND		= 0x01,
	RC632_REG_FIFO_DATA		= 0x02,
	RC632_REG_PRIMARY_STATUS	= 0x03,
	RC632_REG_FIFO_LENGTH		= 0x04,
	RC632_REG_SECONDARY_STATUS	= 0x05,
	RC632_REG_INTERRUPT_EN		= 0x06,
	RC632_REG_INTERRUPT_RQ		= 0x07,

	RC632_REG_PAGE1			= 0x08,
	RC632_REG_CONTROL		= 0x09,
	RC632_REG_ERROR_FLAG		= 0x0a,
	RC632_REG_COLL_POS		= 0x0b,
	RC632_REG_TIMER_VALUE		= 0x0c,
	RC632_REG_CRC_RESULT_LSB	= 0x0d,
	RC632_REG_CRC_RESULT_MSB	= 0x0e,
	RC632_REG_BIT_FRAMING		= 0x0f,

	RC632_REG_PAGE2			= 0x10,
	RC632_REG_TX_CONTROL		= 0x11,
	RC632_REG_CW_CONDUCTANCE	= 0x12,
	RC632_REG_MOD_CONDUCTANCE	= 0x13,
	RC632_REG_CODER_CONTROL		= 0x14,
	RC632_REG_MOD_WIDTH		= 0x15,
	RC632_REG_MOD_WIDTH_SOF		= 0x16,
	RC632_REG_TYPE_B_FRAMING	= 0x17,

	RC632_REG_PAGE3			= 0x18,
	RC632_REG_RX_CONTROL1		= 0x19,
	RC632_REG_DECODER_CONTROL	= 0x1a,
	RC632_REG_BIT_PHASE		= 0x1b,
	RC632_REG_RX_THRESHOLD		= 0x1c,
	RC632_REG_BPSK_DEM_CONTROL	= 0x1d,
	RC632_REG_RX_CONTROL2		= 0x1e,
	RC632_REG_CLOCK_Q_CONTROL	= 0x1f,

	RC632_REG_PAGE4			= 0x20,
	RC632_REG_RX_WAIT		= 0x21,
	RC632_REG_CHANNEL_REDUNDANCY	= 0x22,
	RC632_REG_CRC_PRESET_LSB	= 0x23,
	RC632_REG_CRC_PRESET_MSB	= 0x24,
	RC632_REG_TIME_SLOT_PERIOD	= 0x25,
	RC632_REG_MFOUT_SELECT		= 0x26,
	RC632_REG_PRESET_27		= 0x27,

	RC632_REG_PAGE5			= 0x28,
	RC632_REG_FIFO_LEVEL		= 0x29,
	RC632_REG_TIMER_CLOCK		= 0x2a,
	RC632_REG_TIMER_CONTROL		= 0x2b,
	RC632_REG_TIMER_RELOAD		= 0x2c,
	RC632_REG_IRQ_PIN_CONFIG	= 0x2d,
	RC632_REG_PRESET_2E		= 0x2e,
	RC632_REG_PRESET_2F		= 0x2f,

	RC632_REG_PAGE6			= 0x30,

	RC632_REG_PAGE7			= 0x38,
	RC632_REG_TEST_ANA_SELECT	= 0x3a,
	RC632_REG_TEST_DIGI_SELECT	= 0x3d,
};

enum rc632_reg_status {
	RC632_STAT_LOALERT		= 0x01,
	RC632_STAT_HIALERT		= 0x02,
	RC632_STAT_ERR			= 0x04,
	RC632_STAT_IRQ			= 0x08,
#define RC632_STAT_MODEM_MASK		0x70
	RC632_STAT_MODEM_IDLE		= 0x00,
	RC632_STAT_MODEM_TXSOF		= 0x10,
	RC632_STAT_MODEM_TXDATA		= 0x20,
	RC632_STAT_MODEM_TXEOF		= 0x30,
	RC632_STAT_MODEM_GOTORX		= 0x40,
	RC632_STAT_MODEM_PREPARERX	= 0x50,
	RC632_STAT_MODEM_AWAITINGRX	= 0x60,
	RC632_STAT_MODEM_RECV		= 0x70,
};

enum rc632_reg_command {
	RC632_CMD_IDLE			= 0x00,
	RC632_CMD_WRITE_E2		= 0x01,
	RC632_CMD_READ_E2		= 0x03,
	RC632_CMD_LOAD_CONFIG		= 0x07,
	RC632_CMD_LOAD_KEY_E2		= 0x0b,
	RC632_CMD_AUTHENT1		= 0x0c,
	RC632_CMD_CALC_CRC		= 0x12,
	RC632_CMD_AUTHENT2		= 0x14,
	RC632_CMD_RECEIVE		= 0x16,
	RC632_CMD_LOAD_KEY		= 0x19,
	RC632_CMD_TRANSMIT		= 0x1a,
	RC632_CMD_TRANSCEIVE		= 0x1e,
	RC632_CMD_STARTUP		= 0x3f,
};

enum rc632_reg_interrupt {
	RC632_INT_LOALERT		= 0x01,
	RC632_INT_HIALERT		= 0x02,
	RC632_INT_IDLE			= 0x04,
	RC632_INT_RX			= 0x08,
	RC632_INT_TX			= 0x10,
	RC632_INT_TIMER			= 0x20,
	RC632_INT_SET			= 0x80,
};

enum rc632_reg_control {
	RC632_CONTROL_FIFO_FLUSH	= 0x01,
	RC632_CONTROL_TIMER_START	= 0x02,
	RC632_CONTROL_TIMER_STOP	= 0x04,
	RC632_CONTROL_CRYPTO1_ON	= 0x08,
	RC632_CONTROL_POWERDOWN		= 0x10,
	RC632_CONTROL_STANDBY		= 0x20,
};

enum rc632_reg_error_flag {
	RC632_ERR_FLAG_COL_ERR		= 0x01,
	RC632_ERR_FLAG_PARITY_ERR	= 0x02,
	RC632_ERR_FLAG_FRAMING_ERR	= 0x04,
	RC632_ERR_FLAG_CRC_ERR		= 0x08,
	RC632_ERR_FLAG_FIFO_OVERFLOW	= 0x10,
	RC632_ERR_FLAG_ACCESS_ERR	= 0x20,
	RC632_ERR_FLAG_KEY_ERR		= 0x40,
};

enum rc632_reg_tx_control {
	RC632_TXCTRL_TX1_RF_EN		= 0x01,
	RC632_TXCTRL_TX2_RF_EN		= 0x02,
	RC632_TXCTRL_TX2_CW		= 0x04,
	RC632_TXCTRL_TX2_INV		= 0x08,
	RC632_TXCTRL_FORCE_100_ASK	= 0x10,

	RC632_TXCTRL_MOD_SRC_LOW	= 0x00,
	RC632_TXCTRL_MOD_SRC_HIGH	= 0x20,
	RC632_TXCTRL_MOD_SRC_INT	= 0x40,
	RC632_TXCTRL_MOD_SRC_MFIN	= 0x60,
};

enum rc632_reg_coder_control {
	 /* bit 2-0 TXCoding */
#define RC632_CDRCTRL_TXCD_MASK		0x07
	RC632_CDRCTRL_TXCD_NRZ		= 0x00,
	RC632_CDRCTRL_TXCD_14443A	= 0x01,
	RC632_CDRCTRL_TXCD_ICODE_STD	= 0x04,
	RC632_CDRCTRL_TXCD_ICODE_FAST	= 0x05,
	RC632_CDRCTRL_TXCD_15693_STD	= 0x06,
	RC632_CDRCTRL_TXCD_15693_FAST	= 0x07,

	/* bit5-3 CoderRate*/
#define RC632_CDRCTRL_RATE_MASK		0x38
	RC632_CDRCTRL_RATE_848K		= 0x00,
	RC632_CDRCTRL_RATE_424K		= 0x08,
	RC632_CDRCTRL_RATE_212K		= 0x10,
	RC632_CDRCTRL_RATE_106K		= 0x18,
	RC632_CDRCTRL_RATE_14443B	= 0x20,
	RC632_CDRCTRL_RATE_15693	= 0x28,
	RC632_CDRCTRL_RATE_ICODE_FAST	= 0x30,

	/* bit 7 SendOnePuls */
	RC632_CDRCTRL_15693_EOF_PULSE	= 0x80,
};

enum rc632_erg_type_b_framing {
	RC632_TBFRAMING_SOF_10L_2H	= 0x00,
	RC632_TBFRAMING_SOF_10L_3H	= 0x01,
	RC632_TBFRAMING_SOF_11L_2H	= 0x02,
	RC632_TBFRAMING_SOF_11L_3H	= 0x03,

	RC632_TBFRAMING_EOF_10		= 0x00,
	RC632_TBFRAMING_EOF_11		= 0x20,

	RC632_TBFRAMING_NO_TX_SOF	= 0x80,
	RC632_TBFRAMING_NO_TX_EOF	= 0x40,
};
#define	RC632_TBFRAMING_SPACE_SHIFT	2
#define RC632_TBFRAMING_SPACE_MASK	7

enum rc632_reg_rx_control1 {
	RC632_RXCTRL1_GAIN_20DB		= 0x00,
	RC632_RXCTRL1_GAIN_24DB		= 0x01,
	RC632_RXCTRL1_GAIN_31DB		= 0x02,
	RC632_RXCTRL1_GAIN_35DB		= 0x03,

	RC632_RXCTRL1_LP_OFF		= 0x04,
	RC632_RXCTRL1_ISO15693		= 0x08,
	RC632_RXCTRL1_ISO14443		= 0x10,

#define RC632_RXCTRL1_SUBCP_MASK	0xe0
	RC632_RXCTRL1_SUBCP_1		= 0x00,
	RC632_RXCTRL1_SUBCP_2		= 0x20,
	RC632_RXCTRL1_SUBCP_4		= 0x40,
	RC632_RXCTRL1_SUBCP_8		= 0x60,
	RC632_RXCTRL1_SUBCP_16		= 0x80,
};

enum rc632_reg_decoder_control {
	RC632_DECCTRL_MANCHESTER	= 0x00,
	RC632_DECCTRL_BPSK		= 0x01,

	RC632_DECCTRL_RX_INVERT		= 0x04,

	RC632_DECCTRL_RXFR_ICODE	= 0x00,
	RC632_DECCTRL_RXFR_14443A	= 0x08,
	RC632_DECCTRL_RXFR_15693	= 0x10,
	RC632_DECCTRL_RXFR_14443B	= 0x18,

	RC632_DECCTRL_ZEROAFTERCOL	= 0x20,

	RC632_DECCTRL_RX_MULTIPLE	= 0x40,
};

enum rc632_reg_bpsk_dem_control {
	RC632_BPSKD_TAUB_SHIFT		= 0x00,
	RC632_BPSKD_TAUB_MASK		= 0x03,
	
	RC632_BPSKD_TAUD_SHIFT		= 0x02,
	RC632_BPSKD_TAUD_MASK		= 0x03,

	RC632_BPSKD_FILTER_AMP_DETECT	= 0x10,
	RC632_BPSKD_NO_RX_EOF		= 0x20,
	RC632_BPSKD_NO_RX_EGT		= 0x40,
	RC632_BPSKD_NO_RX_SOF		= 0x80,
};

enum rc632_reg_rx_control2 {
	RC632_RXCTRL2_DECSRC_LOW	= 0x00,
	RC632_RXCTRL2_DECSRC_INT	= 0x01,
	RC632_RXCTRL2_DECSRC_SUBC_MFIN	= 0x10,
	RC632_RXCTRL2_DECSRC_BASE_MFIN	= 0x11,

	RC632_RXCTRL2_AUTO_PD		= 0x40,
	RC632_RXCTRL2_CLK_I		= 0x80,
	RC632_RXCTRL2_CLK_Q		= 0x00,
};

enum rc632_reg_channel_redundancy {
	RC632_CR_PARITY_ENABLE		= 0x01,
	RC632_CR_PARITY_ODD		= 0x02,
	RC632_CR_TX_CRC_ENABLE		= 0x04,
	RC632_CR_RX_CRC_ENABLE		= 0x08,
	RC632_CR_CRC8			= 0x10,
	RC632_CR_CRC3309		= 0x20,
};

enum rc632_reg_timer_control {
	RC632_TMR_START_TX_BEGIN	= 0x01,
	RC632_TMR_START_TX_END		= 0x02,
	RC632_TMR_STOP_RX_BEGIN		= 0x04,
	RC632_TMR_STOP_RX_END		= 0x08,
};
 
enum rc632_reg_timer_irq {
	RC632_IRQ_LO_ALERT		= 0x01,
	RC632_IRQ_HI_ALERT		= 0x02,
	RC632_IRQ_IDLE			= 0x04,
	RC632_IRQ_RX			= 0x08,
	RC632_IRQ_TX			= 0x10,
	RC632_IRQ_TIMER			= 0x20,

	RC632_IRQ_SET			= 0x80,
};

enum rc632_reg_secondary_status {
	RC632_SEC_ST_TMR_RUNNING	= 0x80,
	RC632_SEC_ST_E2_READY		= 0x40,
	RC632_SEC_ST_CRC_READY		= 0x20,
};
