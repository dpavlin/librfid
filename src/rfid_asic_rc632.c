/* Generic Philips CL RC632 Routines
 *
 * (C) 2005-2008 Harald Welte <laforge@gnumonks.org>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>

#include <librfid/rfid.h>
#include <librfid/rfid_asic.h>
#include <librfid/rfid_asic_rc632.h>
#include <librfid/rfid_reader_cm5121.h>
#include <librfid/rfid_layer2_iso14443a.h>
#include <librfid/rfid_layer2_iso15693.h>
#include <librfid/rfid_protocol_mifare_classic.h>

#include "rfid_iso14443_common.h"
#include "rc632.h"

#ifdef  __MINGW32__
#include "usleep.h"
#endif/*__MINGW32__*/

#define RC632_TMO_AUTH1	140

#define TIMER_RELAX_FACTOR	10

#define ENTER()		DEBUGP("entering\n")
const struct rfid_asic rc632;

struct register_file {
	u_int8_t reg;
	u_int8_t val;
};

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
rc632_clear_irqs(struct rfid_asic_handle *handle, u_int8_t bits)
{
	return rc632_reg_write(handle, RC632_REG_INTERRUPT_RQ, (~RC632_INT_SET)&bits);
}

static int 
rc632_rf_power(struct rfid_asic_handle *handle, int on)
{
	ENTER();
	if (on)
		return rc632_set_bits(handle, RC632_REG_TX_CONTROL,
				      RC632_TXCTRL_TX1_RF_EN|
				      RC632_TXCTRL_TX2_RF_EN);
	else
		return rc632_clear_bits(handle, RC632_REG_TX_CONTROL,
					RC632_TXCTRL_TX1_RF_EN|
					RC632_TXCTRL_TX2_RF_EN);
}

static int
rc632_power(struct rfid_asic_handle *handle, int on)
{
	ENTER();
	if (on)
		return rc632_clear_bits(handle, RC632_REG_CONTROL, 
					RC632_CONTROL_POWERDOWN);
	else
		return rc632_set_bits(handle, RC632_REG_CONTROL,
				      RC632_CONTROL_POWERDOWN);
}

static int
rc632_execute_script(struct rfid_asic_handle *h, struct register_file *f,
		     int len)
{
	int i, ret;

	for (i = 0; i < len; i++) {
		ret = rc632_reg_write(h, f[i].reg, f[i].val);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/* calculate best 8bit prescaler and divisor for given usec timeout */
static int best_prescaler(u_int64_t timeout, u_int8_t *prescaler,
			  u_int8_t *divisor)
{
	u_int8_t best_prescaler, best_divisor, i;
	int64_t smallest_diff;

	smallest_diff = LLONG_MAX;
	best_prescaler = 0;

	for (i = 0; i < 21; i++) {
		u_int64_t clk, tmp_div, res;
		int64_t diff;
		clk = 13560000 / (1 << i);
		tmp_div = (clk * timeout) / 1000000;
		tmp_div++;

		if ((tmp_div > 0xff) || (tmp_div > clk))
			continue;

		res = 1000000 / (clk / tmp_div);
		diff = res - timeout;

		if (diff < 0)
			continue;

		if (diff < smallest_diff) {
			best_prescaler = i;
			best_divisor = tmp_div;
			smallest_diff = diff;
		}
	}

	*prescaler = best_prescaler;
	*divisor = best_divisor;

	DEBUGP("timeout %u usec, prescaler = %u, divisor = %u\n",
		timeout, best_prescaler, best_divisor);

	return 0;
}

static int
rc632_timer_set(struct rfid_asic_handle *handle,
		u_int64_t timeout)
{
	int ret;
	u_int8_t prescaler, divisor, irq;

	timeout *= TIMER_RELAX_FACTOR;

	ret = best_prescaler(timeout, &prescaler, &divisor);

	ret = rc632_reg_write(handle, RC632_REG_TIMER_CLOCK,
			      prescaler & 0x1f);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_TIMER_CONTROL,
			      RC632_TMR_START_TX_END|RC632_TMR_STOP_RX_BEGIN);

	/* clear timer irq bit */
	ret = rc632_clear_irqs(handle, RC632_IRQ_TIMER);

	/* enable timer IRQ */
	ret |= rc632_reg_write(handle, RC632_REG_INTERRUPT_EN, RC632_IRQ_SET | RC632_IRQ_TIMER);

	ret |= rc632_reg_write(handle, RC632_REG_TIMER_RELOAD, divisor);

	return ret;
}

/* Wait until RC632 is idle or TIMER IRQ has happened */
static int rc632_wait_idle_timer(struct rfid_asic_handle *handle)
{
	int ret;
	u_int8_t stat, irq, cmd;

	ret = rc632_reg_read(handle, RC632_REG_INTERRUPT_EN, &irq);
	if (ret < 0)
		return ret;
	DEBUGP_INTERRUPT_FLAG("irq_en",irq);

	ret = rc632_reg_write(handle, RC632_REG_INTERRUPT_EN, RC632_IRQ_SET
				| RC632_IRQ_TIMER
				| RC632_IRQ_IDLE
				| RC632_IRQ_RX );
	if (ret < 0)
		return ret;

	while (1) {
		rc632_reg_read(handle, RC632_REG_PRIMARY_STATUS, &stat);
		DEBUGP_STATUS_FLAG(stat);
		if (stat & RC632_STAT_ERR) {
			u_int8_t err;
			ret = rc632_reg_read(handle, RC632_REG_ERROR_FLAG, &err);
			if (ret < 0)
				return ret;
			DEBUGP_ERROR_FLAG(err);
			if (err & (RC632_ERR_FLAG_COL_ERR |
				   RC632_ERR_FLAG_PARITY_ERR |
				   RC632_ERR_FLAG_FRAMING_ERR |
				/* FIXME: why get we CRC errors in CL2 anticol at iso14443a operation with mifare UL? */
				/*   RC632_ERR_FLAG_CRC_ERR | */
				   0))
				return -EIO;
		}
		if (stat & RC632_STAT_IRQ) {
			ret = rc632_reg_read(handle, RC632_REG_INTERRUPT_RQ, &irq);
			if (ret < 0)
				return ret;
			DEBUGP_INTERRUPT_FLAG("irq_rq",irq);

			if (irq & RC632_IRQ_TIMER && !(irq & RC632_IRQ_RX)) {
				DEBUGP("timer expired before RX!!\n");
				rc632_clear_irqs(handle, RC632_IRQ_TIMER);
				return -ETIMEDOUT;
			}
		}

		ret = rc632_reg_read(handle, RC632_REG_COMMAND, &cmd);
		if (ret < 0)
			return ret;

		if (cmd == 0) {
			rc632_clear_irqs(handle, RC632_IRQ_RX);
			return 0;
		}

		/* poll every millisecond */
		usleep(1000);
	}
}

/* Stupid RC632 implementations don't evaluate interrupts but poll the
 * command register for "status idle" */
static int
rc632_wait_idle(struct rfid_asic_handle *handle, u_int64_t timeout)
{
	u_int8_t cmd = 0xff;
	int ret, cycles = 0;
#define USLEEP_PER_CYCLE	128

	timeout *= TIMER_RELAX_FACTOR;

	while (cmd != 0) {
		ret = rc632_reg_read(handle, RC632_REG_COMMAND, &cmd);
		if (ret < 0)
			return ret;

		{
			u_int8_t foo;
			rc632_reg_read(handle, RC632_REG_PRIMARY_STATUS, &foo);
			DEBUGP_STATUS_FLAG(foo);
 			/* check if Error has occured (ERR flag set) */
			if (foo & RC632_STAT_ERR) {
				rc632_reg_read(handle, RC632_REG_ERROR_FLAG, &foo);
				DEBUGP_ERROR_FLAG(foo);
			}
			/* check if IRQ has occurred (IRQ flag set)*/
			if (foo & RC632_STAT_IRQ) { 
				ret = rc632_reg_read(handle, RC632_REG_INTERRUPT_RQ, &foo);
				DEBUGP_INTERRUPT_FLAG("irq_rq",foo);
				/* clear all interrupts */
				ret = rc632_clear_irqs(handle, 0xff);
				if (ret < 0)
					return ret;
			}
		}
		if (cmd == 0) {
			/* FIXME: read second time ?? */
			DEBUGP("cmd == 0 (IDLE)\n");
			return 0;
		}

		/* Abort after some timeout */
		if (cycles > timeout/USLEEP_PER_CYCLE) {
			DEBUGP("timeout...\n");
			return -ETIMEDOUT;
		}

		cycles++;
		usleep(USLEEP_PER_CYCLE);
	}

	return 0;
}

static int
rc632_transmit(struct rfid_asic_handle *handle,
		const u_int8_t *buf,
		u_int8_t len,
		u_int64_t timeout)
{
	int ret, cur_len;
	const u_int8_t *cur_buf = buf;

	DEBUGP("timeout=%u, tx_len=%u\n", timeout, len);

	if (len > 64)
		cur_len = 64;
	else
		cur_len = len;
	
	do {
		ret = rc632_fifo_write(handle, cur_len, cur_buf, 0x03);
		if (ret < 0)
			return ret;

		if (cur_buf == buf)  {
			/* only start transmit first time */
			ret = rc632_reg_write(handle, RC632_REG_COMMAND,
					      RC632_CMD_TRANSMIT);
			if (ret < 0)
				return ret;
		}

		cur_buf += cur_len;
		if (cur_buf < buf + len) {
			cur_len = buf - cur_buf;
			if (cur_len > 64)
				cur_len = 64;
		} else
			cur_len = 0;

	} while (cur_len);

	return rc632_wait_idle(handle, timeout);
}

static int
tcl_toggle_pcb(struct rfid_asic_handle *handle)
{
	/* FIXME: toggle something between 0x0a and 0x0b */
	return 0;
}

static int
rc632_transceive(struct rfid_asic_handle *handle,
		 const u_int8_t *tx_buf,
		 u_int8_t tx_len,
		 u_int8_t *rx_buf,
		 u_int8_t *rx_len,
		 u_int64_t timer,
		 unsigned int toggle)
{
	int ret, cur_tx_len, i;
	u_int8_t rx_avail;
	const u_int8_t *cur_tx_buf = tx_buf;

	DEBUGP("timeout=%u, rx_len=%u, tx_len=%u\n", timer, *rx_len, tx_len);

	if (tx_len > 64)
		cur_tx_len = 64;
	else
		cur_tx_len = tx_len;


	ret = rc632_reg_write(handle, RC632_REG_COMMAND, RC632_CMD_IDLE);
	/* clear all interrupts */
	ret = rc632_reg_write(handle, RC632_REG_INTERRUPT_RQ, 0x7f);

	{ u_int8_t tmp;
	rc632_reg_read(handle, RC632_REG_PRIMARY_STATUS, &tmp);
	DEBUGP_STATUS_FLAG(tmp);
	rc632_reg_read(handle, RC632_REG_ERROR_FLAG, &tmp);
	DEBUGP_ERROR_FLAG(tmp);
	}

	ret = rc632_timer_set(handle, timer);
	if (ret < 0)
		return ret;
	
	do {	
		ret = rc632_fifo_write(handle, cur_tx_len, cur_tx_buf, 0x03);
		if (ret < 0)
			return ret;

		if (cur_tx_buf == tx_buf) {
			ret = rc632_reg_write(handle, RC632_REG_COMMAND,
					      RC632_CMD_TRANSCEIVE);
			if (ret < 0)
				return ret;
		}

		cur_tx_buf += cur_tx_len;
		if (cur_tx_buf < tx_buf + tx_len) {
			u_int8_t fifo_fill;
			ret = rc632_reg_read(handle, RC632_REG_FIFO_LENGTH,
					     &fifo_fill);
			if (ret < 0)
				return ret;

			cur_tx_len = 64 - fifo_fill;
		} else
			cur_tx_len = 0;

	} while (cur_tx_len);

	if (toggle == 1)
		tcl_toggle_pcb(handle);

	ret = rc632_wait_idle_timer(handle);
	//ret = rc632_wait_idle(handle, timer);

	DEBUGP("rc632_wait_idle >> ret=%d %s\n",ret,(ret==-ETIMEDOUT)?"ETIMEDOUT":"");
	if (ret < 0)
		return ret;

	ret = rc632_reg_read(handle, RC632_REG_FIFO_LENGTH, &rx_avail);
	if (ret < 0)
		return ret;

	if (rx_avail > *rx_len)
		DEBUGP("rx_avail(%d) > rx_len(%d), JFYI\n", rx_avail, *rx_len);
	else if (*rx_len > rx_avail)
		*rx_len = rx_avail;

	DEBUGP("rx_len == %d\n",*rx_len);

	if (rx_avail == 0) {
		u_int8_t tmp;

		for (i = 0; i < 1; i++){
			rc632_reg_read(handle, RC632_REG_PRIMARY_STATUS, &tmp);
			DEBUGP_STATUS_FLAG(tmp);
			rc632_reg_read(handle, RC632_REG_ERROR_FLAG, &tmp);
			DEBUGP_ERROR_FLAG(tmp);
		}
		rc632_reg_read(handle, RC632_REG_CHANNEL_REDUNDANCY, &tmp);

		//return 0;
		return -EIO;
	}

	return rc632_fifo_read(handle, *rx_len, rx_buf);
	/* FIXME: discard addidional bytes in FIFO */
}


static int
rc632_receive(struct rfid_asic_handle *handle,
		 u_int8_t *rx_buf,
		 u_int8_t *rx_len,
		 u_int64_t timer)
{
	int ret, cur_tx_len, i;
	u_int8_t rx_avail;

	DEBUGP("timeout=%u, rx_len=%u\n", timer, *rx_len);
	ret = rc632_reg_write(handle, RC632_REG_COMMAND, 0x00); /* IDLE */
	/* clear all interrupts */
	ret = rc632_reg_write(handle, RC632_REG_INTERRUPT_RQ, 0x7f);

	ret = rc632_timer_set(handle, timer);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_COMMAND,RC632_CMD_RECEIVE);
	if (ret < 0)
		return ret;
	
	/* the timer cannot start in hardware based on the command we just
	 * sent. this means that our timing will always be quite a bit more lax,
	 * i.e. we'll always wait for a bit longer than the specs ask us to. */
	ret = rc632_set_bits(handle, RC632_REG_CONTROL,
			     RC632_CONTROL_TIMER_START);
	if (ret < 0)
		return ret;

	//ret = rc632_wait_idle(handle, timer);
	ret = rc632_wait_idle_timer(handle);
	if (ret < 0)
		return ret;

	ret = rc632_reg_read(handle, RC632_REG_FIFO_LENGTH, &rx_avail);
	if (ret < 0)
		return ret;

	if (rx_avail > *rx_len) {
		//DEBUGP("rx_avail(%d) > rx_len(%d), JFYI\n", rx_avail, *rx_len);
	} else if (*rx_len > rx_avail)
		*rx_len = rx_avail;

	if (rx_avail == 0) {
		u_int8_t tmp;

		DEBUGP("rx_len == 0\n");

		for (i = 0; i < 1; i++) {
			rc632_reg_read(handle, RC632_REG_PRIMARY_STATUS, &tmp);
			DEBUGP_STATUS_FLAG(tmp);
			rc632_reg_read(handle, RC632_REG_ERROR_FLAG, &tmp);
			DEBUGP_ERROR_FLAG(tmp);
		}

		rc632_reg_read(handle, RC632_REG_CHANNEL_REDUNDANCY, &tmp);
		return -1; 
	}

	return rc632_fifo_read(handle, *rx_len, rx_buf);
	/* FIXME: discard additional bytes in FIFO */
}

#define MAX_WRITE_LEN	16	/* see Sec. 18.6.1.2 of RC632 Spec Rev. 3.2. */

static int
rc632_write_eeprom(struct rfid_asic_handle *handle, u_int16_t addr, 
		   u_int8_t *data, u_int8_t len)
{
	u_int8_t sndbuf[MAX_WRITE_LEN + 2];
	u_int8_t reg;
	int ret;

	if (len > MAX_WRITE_LEN)
		return -EINVAL;
	if (addr < 0x10)
		return -EPERM;
	if (addr > 0x1ff)
		return -EINVAL;

	sndbuf[0] = addr & 0x00ff;	/* LSB */
	sndbuf[1] = addr >> 8;		/* MSB */
	memcpy(&sndbuf[2], data, len);

	ret = rc632_fifo_write(handle, len + 2, sndbuf, 0x03);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_COMMAND, RC632_CMD_WRITE_E2);
	if (ret < 0)
		return ret;
	
	ret = rc632_reg_read(handle, RC632_REG_ERROR_FLAG, &reg);
	if (ret < 0)
		return ret;

	if (reg & RC632_ERR_FLAG_ACCESS_ERR)
		return -EPERM;

	while (1) {
		u_int8_t reg;
		ret = rc632_reg_read(handle, RC632_REG_SECONDARY_STATUS, &reg);
		if (ret < 0)
			return ret;

		if (reg & RC632_SEC_ST_E2_READY) {
			/* the E2Write command must be terminated, See sec. 18.6.1.3 */
			ret = rc632_reg_write(handle, RC632_REG_COMMAND, RC632_CMD_IDLE);
			break;
		}
	}
	
	return ret;
}

static int
rc632_read_eeprom(struct rfid_asic_handle *handle, u_int16_t addr,
		  u_int8_t *buf, u_int8_t len)
{
	u_int8_t sndbuf[3];
	u_int8_t reg;
	int ret;

	sndbuf[0] = addr & 0xff;
	sndbuf[1] = addr >> 8;
	sndbuf[2] = len;

	ret = rc632_fifo_write(handle, 3, sndbuf, 0x03);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(handle, RC632_REG_COMMAND, RC632_CMD_READ_E2);
	if (ret < 0)
		return ret;

	ret = rc632_reg_read(handle, RC632_REG_ERROR_FLAG, &reg);
	if (ret < 0)
		return ret;

	if (reg & RC632_ERR_FLAG_ACCESS_ERR)
		return -EPERM;

	usleep(20000);

	return rc632_fifo_read(handle, len, buf);
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
	
	usleep(10000);	/* FIXME: no checking for cmd completion? *

	ret = rc632_reg_read(handle, RC632_REG_CRC_RESULT_LSB, &crc_lsb);
	if (ret < 0)
		return ret;

	ret = rc632_reg_read(handle, RC632_REG_CRC_RESULT_MSB, &crc_msb);
	if (ret < 0)
		return ret;

	/* FIXME: what to do with crc result? */
	return ret;
}


int
rc632_register_dump(struct rfid_asic_handle *handle, u_int8_t *buf)
{
	int ret = 0;
	u_int8_t i;

	for (i = 0; i <= 0x3f; i++)
		ret |= rc632_reg_read(handle, i, &buf[i]);

	return ret;
}

/* generic FIFO access functions (if no more efficient ones provided by
 * transport driver) */

static int 
generic_fifo_write()
{
	/* FIXME: implementation (not needed for CM 5121) */
	return -1;
}

static int
generic_fifo_read()
{
	/* FIXME: implementation (not neded for CM 5121) */
	return -1;
}

static int
rc632_init(struct rfid_asic_handle *ah)
{
	int ret;

	/* switch off rf (make sure PICCs are reset at init time) */
	ret = rc632_power(ah, 0);
	if (ret < 0)
		return ret;

	usleep(10000);

	/* switch on rf */
	ret = rc632_power(ah, 1);
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

	/* switch off rf */
	ret = rc632_rf_power(ah, 0);
	if (ret < 0)
		return ret;

	usleep(100000);

	/* switch on rf */
	ret = rc632_rf_power(ah, 1);
	if (ret < 0)
		return ret;

	return 0;
}

static int
rc632_fini(struct rfid_asic_handle *ah)
{
	int ret;

	/* switch off rf */
	ret = rc632_rf_power(ah, 0);
	if (ret < 0)
		return ret;

	ret = rc632_power(ah, 0);
	if (ret < 0)
		return ret;

	return 0;
}

struct rfid_asic_handle *
rc632_open(struct rfid_asic_transport_handle *th)
{
	struct rfid_asic_handle *h;

	h = malloc_asic_handle(sizeof(*h));
	if (!h)
		return NULL;
	memset(h, 0, sizeof(*h));

	h->asic = (void*)&rc632;
	h->rath = th;
	h->fc = h->asic->fc;
	/* FIXME: this is only cm5121 specific, since the latency
	 * down to the RC632 FIFO is too long to refill during TX/RX */
	h->mtu = h->mru = 64;

	if (rc632_init(h) < 0) {
		free_asic_handle(h);
		return NULL;
	}

	return h;
}

void
rc632_close(struct rfid_asic_handle *h)
{
	rc632_fini(h);
	free_asic_handle(h);
}

/*
 * ISO14443A
 */

/* Register file for ISO14443A standard */
static struct register_file iso14443a_script[] = {
	{
		.reg	= RC632_REG_TX_CONTROL,
		.val	= RC632_TXCTRL_MOD_SRC_INT |
			  RC632_TXCTRL_TX2_INV |
			  RC632_TXCTRL_FORCE_100_ASK |
			  RC632_TXCTRL_TX2_RF_EN |
			  RC632_TXCTRL_TX1_RF_EN,
	}, {
		.reg	= RC632_REG_CW_CONDUCTANCE,
		.val	= CM5121_CW_CONDUCTANCE,
	}, {
		.reg	= RC632_REG_MOD_CONDUCTANCE,
		.val	= CM5121_MOD_CONDUCTANCE,
	}, {
		.reg	= RC632_REG_CODER_CONTROL,
		.val	= (RC632_CDRCTRL_TXCD_14443A |
			   RC632_CDRCTRL_RATE_106K),
	}, {
		.reg	= RC632_REG_MOD_WIDTH,
		.val	= 0x13,
	}, {
		.reg	= RC632_REG_MOD_WIDTH_SOF,
		.val	= 0x3f,
	}, {
		.reg	= RC632_REG_TYPE_B_FRAMING,
		.val	= 0x00,
	}, {
		.reg	= RC632_REG_RX_CONTROL1,
		.val	= (RC632_RXCTRL1_GAIN_35DB |
			   RC632_RXCTRL1_ISO14443 |
			   RC632_RXCTRL1_SUBCP_8),
	}, {
		.reg	= RC632_REG_DECODER_CONTROL,
		.val	= (RC632_DECCTRL_MANCHESTER |
			   RC632_DECCTRL_RXFR_14443A),
	}, {
		.reg	= RC632_REG_BIT_PHASE,
		.val	= CM5121_14443A_BITPHASE,
	}, {
		.reg	= RC632_REG_RX_THRESHOLD,
		.val	= CM5121_14443A_THRESHOLD,
	}, {
		.reg	= RC632_REG_BPSK_DEM_CONTROL,
		.val	= 0x00,
	}, {
		.reg	= RC632_REG_RX_CONTROL2,
		.val	= (RC632_RXCTRL2_DECSRC_INT |
			   RC632_RXCTRL2_CLK_Q),
	}, {
		.reg	= RC632_REG_RX_WAIT,
		//.val	= 0x03, /* default value */
		.val	= 0x06,	/* omnikey */
	}, {
		.reg	= RC632_REG_CHANNEL_REDUNDANCY,
		.val	= (RC632_CR_PARITY_ENABLE |
			   RC632_CR_PARITY_ODD),
	}, {
		.reg	= RC632_REG_CRC_PRESET_LSB,
		.val	= 0x63,
	}, {
		.reg	= RC632_REG_CRC_PRESET_MSB,
		.val	= 0x63,
	},
};

static int
rc632_iso14443a_init(struct rfid_asic_handle *handle)
{
	int ret;

	/* flush fifo (our way) */
	ret = rc632_reg_write(handle, RC632_REG_CONTROL,
			      RC632_CONTROL_FIFO_FLUSH);

	ret = rc632_execute_script(handle, iso14443a_script,
				   ARRAY_SIZE(iso14443a_script));
	if (ret < 0)
		return ret;

	return 0;
}

static int
rc632_iso14443a_fini(struct iso14443a_handle *handle_14443)
{

#if 0
	ret = rc632_rf_power(handle, 0);
	if (ret < 0)
		return ret;
#endif


	return 0;
}


/* issue a 14443-3 A PCD -> PICC command in a short frame, such as REQA, WUPA */
static int
rc632_iso14443a_transceive_sf(struct rfid_asic_handle *handle,
				u_int8_t cmd,
		    		struct iso14443a_atqa *atqa)
{
	int ret;
	u_int8_t tx_buf[1];
	u_int8_t rx_len = 2;
	u_int8_t error_flag;

	memset(atqa, 0, sizeof(*atqa));

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

	ret = rc632_transceive(handle, tx_buf, sizeof(tx_buf),
				(u_int8_t *)atqa, &rx_len,
				ISO14443A_FDT_ANTICOL_LAST1, 0);
	if (ret < 0) {
		DEBUGP("error during rc632_transceive()\n");
		return ret;
	}

	/* switch back to normal 8bit last byte */
	ret = rc632_reg_write(handle, RC632_REG_BIT_FRAMING, 0x00);
	if (ret < 0)
		return ret;

	/* determine whether there was a collission */
	ret = rc632_reg_read(handle, RC632_REG_ERROR_FLAG, &error_flag);
	if (ret < 0)
		return ret;

	if (error_flag & RC632_ERR_FLAG_COL_ERR) {
		u_int8_t boc;
		/* retrieve bit of collission */
		ret = rc632_reg_read(handle, RC632_REG_COLL_POS, &boc);
		if (ret < 0)
			return ret;
		DEBUGP("collision detected in xcv_sf: bit_of_col=%u\n", boc);
		/* FIXME: how to signal this up the stack */
	}

	if (rx_len != 2) {
		DEBUGP("rx_len(%d) != 2\n", rx_len);
		return -1;
	}

	return 0;
}

/* transceive regular frame */
static int
rc632_iso14443ab_transceive(struct rfid_asic_handle *handle,
			   unsigned int frametype,
			   const u_int8_t *tx_buf, unsigned int tx_len,
			   u_int8_t *rx_buf, unsigned int *rx_len,
			   u_int64_t timeout, unsigned int flags)
{
	int ret;
	u_int8_t rxl;
	u_int8_t channel_red;

	if (*rx_len > 0xff)
		rxl = 0xff;
	else
		rxl = *rx_len;

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
	case RFID_15693_FRAME:
		channel_red = RC632_CR_CRC3309 | RC632_CR_RX_CRC_ENABLE
				| RC632_CR_TX_CRC_ENABLE;
		break;
	case RFID_15693_FRAME_ICODE1:
		/* FIXME: implement */
	default:
		return -EINVAL;
		break;
	}
	ret = rc632_reg_write(handle, RC632_REG_CHANNEL_REDUNDANCY,
			      channel_red);
	if (ret < 0)
		return ret;
	DEBUGP("tx_len=%u\n",tx_len);
	ret = rc632_transceive(handle, tx_buf, tx_len, rx_buf, &rxl, timeout, 0);
	*rx_len = rxl;
	if (ret < 0)
		return ret;


	return 0; 
}

/* transceive anti collission bitframe */
static int
rc632_iso14443a_transceive_acf(struct rfid_asic_handle *handle,
				struct iso14443a_anticol_cmd *acf,
				unsigned int *bit_of_col)
{
	int ret;
	u_int8_t rx_buf[64];
	u_int8_t rx_len = sizeof(rx_buf);
	u_int8_t rx_align = 0, tx_last_bits, tx_bytes, tx_bytes_total;
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

	tx_last_bits = acf->nvb & 0x07;	/* lower nibble indicates bits */
	tx_bytes = ( acf->nvb >> 4 ) & 0x07;
	if (tx_last_bits) {
		tx_bytes_total = tx_bytes+1;
		rx_align = tx_last_bits & 0x07; /* rx frame complements tx */
	}
	else
		tx_bytes_total = tx_bytes;

	/* set RxAlign and TxLastBits*/
	ret = rc632_reg_write(handle, RC632_REG_BIT_FRAMING,
				(rx_align << 4) | (tx_last_bits));
	if (ret < 0)
		return ret;

	ret = rc632_transceive(handle, (u_int8_t *)acf, tx_bytes_total,
				rx_buf, &rx_len, 0x32, 0);
	if (ret < 0)
		return ret;

	/* bitwise-OR the two halves of the split byte */
	acf->uid_bits[tx_bytes-2] = (
		  (acf->uid_bits[tx_bytes-2] & (0xff >> (8-tx_last_bits)))
		| rx_buf[0]);
	
	/* copy the rest */
	if (rx_len)
		memcpy(&acf->uid_bits[tx_bytes-1], &rx_buf[1], rx_len-1);

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
				     unsigned int tx, unsigned int rate)
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

#if 0
static struct register_file iso14443b_script[] = {
	{
		.reg	= RC632_REG_TX_CONTROL,
		.val	= (RC632_TXCTRL_TX1_RF_EN |
			   RC632_TXCTRL_TX2_RF_EN |
			   RC632_TXCTRL_TX2_INV |
			   RC632_TXCTRL_MOD_SRC_INT),
	}, {
		.reg	= RC632_REG_CW_CONDUCTANCE,
		.val	= 0x3f,
	}, {
		.reg	= RC632_REG_MOD_CONDUCTANCE,
		.val	= 0x04,
	}, {
		.reg	= RC632_REG_CODER_CONTROL,
		.val	= (RC632_CDRCTRL_TXCD_NRZ |
			   RC632_CDRCTRL_RATE_14443B),
	}, {
		.reg	= RC632_REG_MOD_WIDTH,
		.val	= 0x13,
	}, {
		.reg	= RC632_REG_MOD_WIDTH_SOF,
		.val	= 0x3f,
	}, {
		.reg	= RC632_REG_TYPE_B_FRAMING,
		.val	= (RC632_TBFRAMING_SOF_11L_3H |
			   (6 << RC632_TBFRAMING_SPACE_SHIFT) |
			   RC632_TBFRAMING_EOF_11);
	}, {
		.reg	= RC632_REG_RX_CONTROL1,
		.val	= (RC632_RXCTRL1_GAIN_35DB |
			   RC632_RXCTRL1_ISO14443,
			   RC632_RXCTRL1_SUBCP_8),
	}, {
		.reg	= RC632_REG_DECODER_CONTROL,
		.val	= (RC632_DECCTRL_BPSK |
			   RC632_DECCTRL_RXFR_14443B),
	}, {
		.reg	= RC632_REG_BIT_PHASE,
		.val	= CM5121_14443B_BITPHASE,
	}, {
		.reg	= RC632_REG_RX_THRESHOLD,
		.val	= CM5121_14443B_THRESHOLD,
	}, {
		.reg	= RC632_REG_BPSK_DEM_CONTROL,
		.val	= ((0x2 & RC632_BPSKD_TAUB_MASK)<<RC632_BPSKD_TAUB_SHIFT |
			   (0x3 & RC632_BPSKD_TAUD_MASK)<<RC632_BPSKD_TAUD_SHIFT |
			   RC632_BPSKD_FILTER_AMP_DETECT |
			   RC632_BPSKD_NO_RX_EOF |
			   RC632_BPSKD_NO_RX_EGT),
	}, {
		.reg	= RC632_REG_RX_CONTROL2,
		.val	= (RC632_RXCTRL2_AUTO_PD |
			   RC632_RXCTRL2_DECSRC_INT),
	}, {
		.reg	= RC632_REG_RX_WAIT,
		.val	= 0x03,
	}, {
		.reg	= RC632_REG_CHANNEL_REDUNDANCY,
		.val	= (RC632_CR_TX_CRC_ENABLE |
			   RC632_CR_RX_CRC_ENABLE |
			   RC632_CR_CRC3309),
	}, {
		.reg	= RC632_REG_CRC_PRESET_LSB,
		.val	= 0xff,
	}, { 
		.reg	= RC632_REG_CRC_PRESET_MSB,
		.val	= 0xff,
	},
};
#endif

static int rc632_iso14443b_init(struct rfid_asic_handle *handle)
{
	int ret;
	ENTER();
	/* FIXME: some FIFO work */
	
	/* flush fifo (our way) */
	ret = rc632_reg_write(handle, RC632_REG_CONTROL,
			      RC632_CONTROL_FIFO_FLUSH);
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



/*
 * ISO15693
 */

/* Register file for ISO15693 standard */
static struct register_file iso15693_fast_script[] = {
	{
		.reg	= RC632_REG_TX_CONTROL,
		.val	= RC632_TXCTRL_MOD_SRC_INT |
			  RC632_TXCTRL_TX2_INV |
			  RC632_TXCTRL_TX2_RF_EN |
			  RC632_TXCTRL_TX1_RF_EN,
	}, {
		.reg	= RC632_REG_CW_CONDUCTANCE,
		.val	= 0x3f,
	}, {
		.reg	= RC632_REG_MOD_CONDUCTANCE,
		/* FIXME: nxp default for icode1/15693: 0x05 */
		//.val	= 0x02,
		.val	= 0x21,	/* omnikey */
	}, {
		.reg	= RC632_REG_CODER_CONTROL,
		.val	= RC632_CDRCTRL_TXCD_15693_FAST |
			  RC632_CDRCTRL_RATE_15693,
	}, {
		.reg	= RC632_REG_MOD_WIDTH,
		.val	= 0x3f,
	}, {
		.reg	= RC632_REG_MOD_WIDTH_SOF,
		.val	= 0x3f,
	}, {
		.reg	= RC632_REG_TYPE_B_FRAMING,
		.val	= 0x00,
	}, {
		.reg	= RC632_REG_RX_CONTROL1,
		.val	= RC632_RXCTRL1_ISO15693 |
			  RC632_RXCTRL1_SUBCP_16 |
			  RC632_RXCTRL1_GAIN_35DB,
	}, {
		/* FIXME: this should always be the case */
		.reg	= RC632_REG_RX_CONTROL2,
		.val	= RC632_RXCTRL2_DECSRC_INT,
	}, {
		.reg	= RC632_REG_DECODER_CONTROL,
		.val	= RC632_DECCTRL_MANCHESTER |
			  RC632_DECCTRL_RX_INVERT |
			  RC632_DECCTRL_ZEROAFTERCOL |
			  RC632_DECCTRL_RXFR_15693,
	}, {
		.reg	= RC632_REG_BIT_PHASE,
		/* FIXME: nxp default for icode1/15693: 0x54 */
		//.val	= 0x52,
		.val	= 0xd0,	/* omnikey */
	}, {
		.reg	= RC632_REG_RX_THRESHOLD,
		/* FIXME: nxp default for icode1/15693: 0x68 */
		//.val	= 0x66,
		.val	= 0xed,
	}, {
		.reg	= RC632_REG_BPSK_DEM_CONTROL,
		.val	= 0x00,
	}, {
		.reg	= RC632_REG_CHANNEL_REDUNDANCY,
		.val	= RC632_CR_RX_CRC_ENABLE |
			  RC632_CR_TX_CRC_ENABLE |
			  RC632_CR_CRC3309,
	}, {
		.reg	= RC632_REG_CRC_PRESET_LSB,
		.val	= 0xff,
	}, {
		.reg	= RC632_REG_CRC_PRESET_MSB,
		.val	= 0xff,
	},
};

/* Register file for I*Code standard */
static struct register_file icode1_std_script[] = {
	{
		.reg	= RC632_REG_TX_CONTROL,
		.val	= RC632_TXCTRL_MOD_SRC_INT |
			  RC632_TXCTRL_TX2_INV |
			  RC632_TXCTRL_TX2_RF_EN |
			  RC632_TXCTRL_TX1_RF_EN,
	}, {
		.reg	= RC632_REG_CW_CONDUCTANCE,
		.val	= 0x3f,
	}, {
		.reg	= RC632_REG_MOD_CONDUCTANCE,
		/* FIXME: nxp default for icode1/15693: 0x05 */
		.val	= 0x02,
	}, {
		.reg	= RC632_REG_CODER_CONTROL,
		.val	= RC632_CDRCTRL_TXCD_ICODE_STD |
			  RC632_CDRCTRL_RATE_15693,
	}, {
		.reg	= RC632_REG_MOD_WIDTH,
		.val	= 0x3f,
	}, {
		.reg	= RC632_REG_MOD_WIDTH_SOF,
		.val	= 0x3f,
	}, {
		.reg	= RC632_REG_TYPE_B_FRAMING,
		.val	= 0x00,
	}, {
		.reg	= RC632_REG_RX_CONTROL1,
		.val	= RC632_RXCTRL1_ISO15693 |
			  RC632_RXCTRL1_SUBCP_16 |
			  RC632_RXCTRL1_GAIN_35DB,
	}, {
		/* FIXME: this should always be the case */
		.reg	= RC632_REG_RX_CONTROL2,
		.val	= RC632_RXCTRL2_DECSRC_INT,
	}, {
		.reg	= RC632_REG_DECODER_CONTROL,
		.val	= RC632_DECCTRL_MANCHESTER |
			  RC632_DECCTRL_RXFR_ICODE,
	}, {
		.reg	= RC632_REG_BIT_PHASE,
		/* FIXME: nxp default for icode1/15693: 0x54 */
		.val	= 0x52,
	}, {
		.reg	= RC632_REG_RX_THRESHOLD,
		/* FIXME: nxp default for icode1/15693: 0x68 */
		.val	= 0x66,
	}, {
		.reg	= RC632_REG_BPSK_DEM_CONTROL,
		.val	= 0x00,
	}, {
		.reg	= RC632_REG_CHANNEL_REDUNDANCY,
		/* 16bit CRC, no parity, not CRC3309 */
		.val	= RC632_CR_RX_CRC_ENABLE |
			  RC632_CR_TX_CRC_ENABLE,
	}, {
		.reg	= RC632_REG_CRC_PRESET_LSB,
		.val	= 0xfe,
	}, {
		.reg	= RC632_REG_CRC_PRESET_MSB,
		.val	= 0xff,
	/* }, {
		.reg	= RC632_REG_INTERRUPT_EN,
		.val	= RC632_INT_IDLE |
			  RC632_INT_TIMER |
			  RC632_INT_RX |
			  RC632_INT_TX, */
	}
};

/* incremental changes on top of icode1_std_script */
static struct register_file icode1_fast_patch[] = {
	{
		.reg	= RC632_REG_CODER_CONTROL,
		.val	= RC632_CDRCTRL_TXCD_ICODE_FAST |
			  RC632_CDRCTRL_RATE_ICODE_FAST,
	}, {
		.reg	= RC632_REG_MOD_WIDTH_SOF,
		.val	= 0x73,	/* 18.88uS */
	},
};


static int
rc632_iso15693_init(struct rfid_asic_handle *h)
{
	int ret;

 	/* flush fifo (our way) */
	ret = rc632_reg_write(h, RC632_REG_CONTROL,
			      RC632_CONTROL_FIFO_FLUSH);
	if (ret < 0)
		return ret;

	ret = rc632_execute_script(h, iso15693_fast_script,
				   ARRAY_SIZE(iso15693_fast_script));
	if (ret < 0)
		return ret;

	return 0;
}

static int
rc632_iso15693_icode1_init(struct rfid_asic_handle *h)
{
	int ret;

	ret = rc632_execute_script(h, icode1_std_script,
				   ARRAY_SIZE(icode1_std_script));
	if (ret < 0)
		return ret;

	/* FIXME: how to configure fast/slow properly? */
#if 0
	if (fast) {
		ret = rc632_execute_script(h, icode1_fast_patch,
					   ARRAY_SIZE(icode1_fast_patch));
		if (ret < 0)
			return ret;
	}
#endif

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

static void uuid_reversecpy(unsigned char* out, unsigned char* in, int len)
{
	int i = 0;
	while (len > 0) {
		out[i] = in[len];
		len--;
		i++;
	}
}

static int
rc632_iso15693_transceive_ac(struct rfid_asic_handle *handle,
			     const struct iso15693_anticol_cmd *acf,
			     unsigned int acf_len,
			     struct iso15693_anticol_resp *resp,
			     unsigned int *rx_len, unsigned char *bit_of_col)
{
	u_int8_t error_flag, boc;
	//u_int8_t rx_len;

	int ret, tx_len, mask_len_bytes;
	unsigned int rate = ISO15693_T_SLOW;

	if (acf->req.flags & RFID_15693_F_RATE_HIGH)
		rate = ISO15693_T_FAST;

	DEBUGP("acf = %s\n", rfid_hexdump(acf, acf_len));

	ret = rc632_transceive(handle, (u_int8_t *)acf, acf_len,
			       (u_int8_t *) resp, rx_len, 
			       iso15693_timing[rate][ISO15693_T1], 0);
	if (ret == -ETIMEDOUT || ret == -EIO)
		return ret;

	/* determine whether there was a collission */
	ret = rc632_reg_read(handle, RC632_REG_ERROR_FLAG, &error_flag);
	if (ret < 0)
		return ret;
	DEBUGP_ERROR_FLAG(error_flag);

	//FIXME: check for framing and crc errors...
	if (error_flag & RC632_ERR_FLAG_COL_ERR) {
		/* retrieve bit of collission */
		ret = rc632_reg_read(handle, RC632_REG_COLL_POS, &boc);
		if (ret < 0)
			return ret;
		*bit_of_col = boc;
	} else {
		*bit_of_col = 0;
		if (error_flag & RC632_ERR_FLAG_CRC_ERR)
			return -EIO;
	}

	return 0;

#if 0
	*bit_of_col = 0;
	
	mask_len_bytes = (acf->mask_len % 8) ? acf->mask_len/8+1 : acf->mask_len/8;

	if (acf->current_slot == 0) {
		/* first call: transmit Inventory frame */
		DEBUGP("first_frame\n");

		tx_len = sizeof(struct iso15693_request) + 1 + mask_len_bytes;

		ret = rc632_transceive(handle, (u_int8_t *)&req, tx_len,
					(u_int8_t *)&rx_buf, &rx_len, ISO15693_T1, 0);
		acf->current_slot = 1;
		DEBUGP("rc632_transceive ret: %d rx_len: %d\n",ret,rx_len);
		/* if ((ret < 0)&&(ret != -ETIMEDOUT))
			return ret;	*/

	} else {
		/* second++ call: end timeslot with EOFpulse and read */
		DEBUGP("second++_frame\n");
		if ((acf->current_slot > 16) ||
		    ((acf->flags & RFID_15693_F5_NSLOTS_1 == 0)
		    			&& (acf->current_slot > 1))) {

			memset(uuid, 0, ISO15693_UID_LEN);
			return -1;
		}

		/* reset EOF-pulse-bit to 0 */
		ret = rc632_clear_bits(handle, RC632_REG_CODER_CONTROL,
				       RC632_CDRCTRL_15693_EOF_PULSE);
		usleep(50);
		/* generate EOF pulse */
		ret = rc632_set_bits(handle, RC632_REG_CODER_CONTROL,
				     RC632_CDRCTRL_15693_EOF_PULSE);
		if (ret < 0)
			return ret;
		// DEBUGP("waiting for EOF pulse\n");
		// ret = rc632_wait_idle(handle, 10); //wait for idle

		rx_len = sizeof(rx_buf);
		ret = rc632_receive(handle, (u_int8_t*)&rx_buf, &rx_len, ISO15693_T3);
		DEBUGP("rc632_receive ret: %d rx_len: %d\n", ret, rx_len);
		acf->current_slot++;

		/* if ((ret < 0)&&(ret != -ETIMEDOUT))
			return ret; */
	}

	rc632_reg_read(handle, RC632_REG_PRIMARY_STATUS, &tmp);
	DEBUGP_STATUS_FLAG(tmp);

	if (ret == -ETIMEDOUT) {
		/* no VICC answer in this timeslot*/
		memset(uuid, 0, ISO15693_UID_LEN);
		return -ETIMEDOUT;
	} else {
		/* determine whether there was a collission */
		ret = rc632_reg_read(handle, RC632_REG_ERROR_FLAG, &error_flag);
		DEBUGP_ERROR_FLAG(error_flag);
		if (ret < 0)
			return ret;

		if (error_flag & RC632_ERR_FLAG_COL_ERR) {
			/* retrieve bit of collission */
			ret = rc632_reg_read(handle, RC632_REG_COLL_POS, &boc);
			if (ret < 0)
				return ret;
			*bit_of_col = boc;
			memcpy(uuid, rx_buf.uuid, ISO15693_UID_LEN);
			// uuid_reversecpy(uuid, rx_buf.uuid, ISO15693_UID_LEN);
			DEBUGP("Collision in slot %d bit %d\n",
				acf->current_slot,boc);
			return -ECOLLISION;
		} else {
			/* no collision-> retrieve uuid */
			DEBUGP("no collision in slot %d\n", acf->current_slot);
			memcpy(uuid, rx_buf.uuid, ISO15693_UID_LEN);
			//uuid_reversecpy(uuid, rx_buf.uuid, ISO15693_UID_LEN);
		}
	} 

	return 0;
#endif
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

	/* Terminate probably running command */
	ret = rc632_reg_write(h, RC632_REG_COMMAND, RC632_CMD_IDLE);	
	if (ret < 0)
		return ret;

	ret = rc632_fifo_write(h, RFID_MIFARE_KEY_CODED_LEN, coded_key, 0x03);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_COMMAND, RC632_CMD_LOAD_KEY);
	if (ret < 0)
		return ret;

	ret = rc632_timer_set(h, RC632_TMO_AUTH1);
	if (ret < 0)
		return ret;

	//ret = rc632_wait_idle(h, RC632_TMO_AUTH1);
	ret = rc632_wait_idle_timer(h);
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
rc632_mifare_set_key_ee(struct rfid_asic_handle *h, unsigned int addr)
{
	int ret;
	u_int8_t cmd_addr[2];
	u_int8_t reg;

	if (addr > 0xffff - RFID_MIFARE_KEY_CODED_LEN)
		return -EINVAL;

	cmd_addr[0] = addr & 0xff;		/* LSB */
	cmd_addr[1] = (addr >> 8) & 0xff;	/* MSB */

	/* Terminate probably running command */
	ret = rc632_reg_write(h, RC632_REG_COMMAND, RC632_CMD_IDLE);	
	if (ret < 0)
		return ret;

	/* Write the key address to the FIFO */
	ret = rc632_fifo_write(h, 2, cmd_addr, 0x03);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_COMMAND, RC632_CMD_LOAD_KEY_E2);
	if (ret < 0)
		return ret;

	ret = rc632_timer_set(h, RC632_TMO_AUTH1);
	if (ret < 0)
		return ret;

	//ret = rc632_wait_idle(h, RC632_TMO_AUTH1);
	ret = rc632_wait_idle_timer(h);
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

	if (cmd != RFID_CMD_MIFARE_AUTH1A && cmd != RFID_CMD_MIFARE_AUTH1B) {
		DEBUGP("invalid auth command\n");
		return -EINVAL;
	}

	/* Initialize acmd */
	acmd.block_address = block & 0xff;
	acmd.auth_cmd = cmd;
	//acmd.serno = htonl(serno);
	acmd.serno = serno;

#if 1
	/* Clear Rx CRC */
	ret = rc632_clear_bits(h, RC632_REG_CHANNEL_REDUNDANCY,
				RC632_CR_RX_CRC_ENABLE);
#else
	/* Clear Rx CRC, Set Tx CRC and Odd Parity */
	ret = rc632_reg_write(h, RC632_REG_CHANNEL_REDUNDANCY,
				RC632_CR_TX_CRC_ENABLE | RC632_CR_PARITY_ODD |
				RC632_CR_PARITY_ENABLE);
#endif
	if (ret < 0)
		return ret;

	/* Send Authent1 Command */
	ret = rc632_fifo_write(h, sizeof(acmd), (unsigned char *)&acmd, 0x03);
	if (ret < 0)
		return ret;

	ret = rc632_reg_write(h, RC632_REG_COMMAND, RC632_CMD_AUTHENT1);
	if (ret < 0) {
		DEBUGP("error during AUTHENT1");
		return ret;
	}

	/* Wait until transmitter is idle */
	ret = rc632_timer_set(h, RC632_TMO_AUTH1);
	if (ret < 0)
		return ret;

	//ret = rc632_wait_idle(h, RC632_TMO_AUTH1);
	ret = rc632_wait_idle_timer(h);
	if (ret < 0)
		return ret;

	ret = rc632_reg_read(h, RC632_REG_SECONDARY_STATUS, &reg);
	if (ret < 0)
		return ret;
	if (reg & 0x07) {
		DEBUGP("bitframe?");
		return -EIO;
	}

	/* Clear Tx CRC */
	ret = rc632_clear_bits(h, RC632_REG_CHANNEL_REDUNDANCY,
				RC632_CR_TX_CRC_ENABLE);
	if (ret < 0)
		return ret;

	/* Wait until transmitter is idle */
	ret = rc632_timer_set(h, RC632_TMO_AUTH1);
	if (ret < 0)
		return ret;

	/* Send Authent2 Command */
	ret = rc632_reg_write(h, RC632_REG_COMMAND, RC632_CMD_AUTHENT2);
	if (ret < 0)
		return ret;

	/* Wait until transmitter is idle */
	//ret = rc632_wait_idle(h, RC632_TMO_AUTH1);
	ret = rc632_wait_idle_timer(h);
	if (ret < 0)
		return ret;

	/* Check whether authentication was successful */
	ret = rc632_reg_read(h, RC632_REG_CONTROL, &reg);
	if (ret < 0)
		return ret;

	if (!(reg & RC632_CONTROL_CRYPTO1_ON)) {
		DEBUGP("authentication not successful");
		return -EACCES;
	}

	return 0;
}

/* transceive regular frame */
static int
rc632_mifare_transceive(struct rfid_asic_handle *handle,
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

	ret = rc632_transceive(handle, tx_buf, tx_len, rx_buf, &rxl, 0x32, 0);
	*rx_len = rxl;
	if (ret < 0)
		return ret;


	return 0; 
}


static int
rc632_layer2_init(struct rfid_asic_handle *h, enum rfid_layer2_id l2)
{
	switch (l2) {
	case RFID_LAYER2_ISO14443A:
		return rc632_iso14443a_init(h);
	case RFID_LAYER2_ISO14443B:
		return rc632_iso14443b_init(h);
	case RFID_LAYER2_ISO15693:
		return rc632_iso15693_init(h);
	case RFID_LAYER2_ICODE1:
		return rc632_iso15693_icode1_init(h);
	default:
		return -EINVAL;
	}
}

const struct rfid_asic rc632 = {
	.name 	= "Philips CL RC632",
	.fc 	= ISO14443_FREQ_CARRIER,
	.priv.rc632 = {
		.fn = {
			.power = &rc632_power,
			.rf_power = &rc632_rf_power,
			.transceive = &rc632_iso14443ab_transceive,
			.init = &rc632_layer2_init,
			.iso14443a = {
				.transceive_sf = &rc632_iso14443a_transceive_sf,
				.transceive_acf = &rc632_iso14443a_transceive_acf,
				.set_speed = &rc632_iso14443a_set_speed,
			},
			.iso15693 = {
				.transceive_ac = &rc632_iso15693_transceive_ac,
			},
			.mifare_classic = {
				.setkey = &rc632_mifare_set_key,
				.setkey_ee = &rc632_mifare_set_key_ee,
				.auth = &rc632_mifare_auth,
			},
		},
	},
};
