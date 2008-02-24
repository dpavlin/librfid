/* Shared/Common functions for all RC632 based readers
 *
 * (C) 2006-2008 by Harald Welte <laforge@gnumonks.org>
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


#include <errno.h>

#include <librfid/rfid.h>
#include <librfid/rfid_reader.h>
#include <librfid/rfid_asic.h>
#include <librfid/rfid_asic_rc632.h>
#include <librfid/rfid_layer2.h>

#include "rfid_reader_rc632_common.h"

int _rdr_rc632_transceive(struct rfid_reader_handle *rh,
			  enum rfid_frametype frametype,
			  const unsigned char *tx_data, unsigned int tx_len,
			  unsigned char *rx_data, unsigned int *rx_len,
			  u_int64_t timeout, unsigned int flags)
{
	return rh->ah->asic->priv.rc632.fn.transceive(rh->ah, frametype,
						      tx_data, tx_len, 
						      rx_data, rx_len,
						      timeout, flags);
}

int _rdr_rc632_transceive_sf(struct rfid_reader_handle *rh,
			     unsigned char cmd, struct iso14443a_atqa *atqa)
{
	return rh->ah->asic->priv.rc632.fn.iso14443a.transceive_sf(rh->ah,
								   cmd,
								   atqa);
}

int
_rdr_rc632_transceive_acf(struct rfid_reader_handle *rh,
			  struct iso14443a_anticol_cmd *cmd,
			  unsigned int *bit_of_col)
{
	return rh->ah->asic->priv.rc632.fn.iso14443a.transceive_acf(rh->ah,
							 cmd, bit_of_col);
}

int
_rdr_rc632_iso15693_transceive_ac(struct rfid_reader_handle *rh,
				  const struct iso15693_anticol_cmd *acf,
				  unsigned int acf_len,
				  struct iso15693_anticol_resp *resp,
				  unsigned int *resp_len, char *bit_of_col)
{
	return 	rh->ah->asic->priv.rc632.fn.iso15693.transceive_ac(
					rh->ah, acf, acf_len, resp, resp_len,
					bit_of_col);
}


int
_rdr_rc632_14443a_set_speed(struct rfid_reader_handle *rh, 
			    unsigned int tx, unsigned int speed)
{
	u_int8_t rate;
	
	DEBUGP("setting rate: ");
	switch (speed) {
	case RFID_14443A_SPEED_106K:
		rate = 0x00;
		DEBUGPC("106K\n");
		break;
	case RFID_14443A_SPEED_212K:
		rate = 0x01;
		DEBUGPC("212K\n");
		break;
	case RFID_14443A_SPEED_424K:
		rate = 0x02;
 		DEBUGPC("424K\n");
		break;
	case RFID_14443A_SPEED_848K:
		rate = 0x03;
		DEBUGPC("848K\n");
		break;
	default:
		return -EINVAL;
		break;
	}
	return rh->ah->asic->priv.rc632.fn.iso14443a.set_speed(rh->ah,
								tx, rate);
}

int
_rdr_rc632_l2_init(struct rfid_reader_handle *rh, enum rfid_layer2_id l2)
{
	return rh->ah->asic->priv.rc632.fn.init(rh->ah, l2);
}

int
_rdr_rc632_mifare_setkey(struct rfid_reader_handle *rh, const u_int8_t *key)
{
	return rh->ah->asic->priv.rc632.fn.mifare_classic.setkey(rh->ah, key);
}

int
_rdr_rc632_mifare_setkey_ee(struct rfid_reader_handle *rh, unsigned int addr)
{
	return rh->ah->asic->priv.rc632.fn.mifare_classic.setkey_ee(rh->ah, addr);
}

int
_rdr_rc632_mifare_auth(struct rfid_reader_handle *rh, u_int8_t cmd, 
		   u_int32_t serno, u_int8_t block)
{
	return rh->ah->asic->priv.rc632.fn.mifare_classic.auth(rh->ah, 
							cmd, serno, block);
}

int
_rdr_rc632_getopt(struct rfid_reader_handle *rh, int optname,
		  void *optval, unsigned int *optlen)
{
	return -EINVAL;
}

int
_rdr_rc632_setopt(struct rfid_reader_handle *rh, int optname,
		  const void *optval, unsigned int optlen)
{
	unsigned int *val = (unsigned int *)optval;

	if (!optval || optlen < sizeof(*val))
		return -EINVAL;

	switch (optname) {
	case RFID_OPT_RDR_RF_KILL:
		if (*val)
			return rh->ah->asic->priv.rc632.fn.rf_power(rh->ah, 0);
		else
			return rh->ah->asic->priv.rc632.fn.rf_power(rh->ah, 1);
	default:
		return -EINVAL;
	}
}
