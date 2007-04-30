#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <librfid/rfid.h>
#include <librfid/rfid_access_mifare_classic.h>

/* parse encoded data structure into c1/c2/c3 */
int mfcl_compile_access(u_int8_t *encoded,
		        const struct mfcl_access_sect *ac)
{
	int b;
	u_int8_t c1, c2, c3;

	c1 = c2 = c3 = 0;

	for (b = 0; b < 4; b++) {
		if (ac->block[b] & 0x01)
			c1 |= (1 << b);
		if (ac->block[b] & 0x02)
			c2 |= (1 << b);
		if (ac->block[b] & 0x04)
			c3 |= (1 << b);
	}

	DEBUGP("compile: c1=%u c2=%u c3=%u\n", c1, c2, c3);

	encoded[3] = 0x00;
	encoded[2] = c2 | (c3 << 4);
	encoded[1] = (~c3 & 0xf) | (c1 << 4);
	encoded[0] = (~c1 & 0xf) | ((~c2 & 0xf) << 4);

	return 0;
}

/* parse mifare classic access conditions from card */
int mfcl_parse_access(struct mfcl_access_sect *ac, u_int8_t *encoded)
{
	int b;

	u_int8_t c1, c2, c3;

	DEBUGP("encoded: %s\n", rfid_hexdump(encoded, 4));

	c1 = encoded[1] >> 4;
	c2 = encoded[2] & 0xf;
	c3 = encoded[2] >> 4;

	DEBUGP("c1=0x%x, c2=0x%x, c3=0x%x\n", c1, c2, c3);
	
	memset(ac, 0, sizeof(*ac));
	for (b = 0; b < 4; b++) {
		if (c1 & (1 << b))
			ac->block[b] = 0x1;
		if (c2 & (1 << b))
			ac->block[b] |= 0x2;
		if (c3 & (1 << b))
			ac->block[b] |= 0x4;
	};

	/* FIXME: verify the inverted access bits */

	return 0;
}


/* functions below here are for our own internal decoded (orthogonal)
 * format of access bits */

/* In the order of the table 3.7.3 in MFCL Product Specification Rev. 5.1 */
static const struct mfcl_access_exp_block mfcl_exp_data[] = {
	[0] = {
		.read	= MFCL_ACCESS_KEY_A | MFCL_ACCESS_KEY_B,
		.write	= MFCL_ACCESS_KEY_A | MFCL_ACCESS_KEY_B,
		.inc	= MFCL_ACCESS_KEY_A | MFCL_ACCESS_KEY_B,
		.dec	= MFCL_ACCESS_KEY_A | MFCL_ACCESS_KEY_B,
	},
	[2] = {
		.read	= MFCL_ACCESS_KEY_A | MFCL_ACCESS_KEY_B,
		.write	= MFCL_ACCESS_NEVER,
		.inc	= MFCL_ACCESS_NEVER,
		.dec	= MFCL_ACCESS_NEVER,
	},
	[1] = {
		.read	= MFCL_ACCESS_KEY_A | MFCL_ACCESS_KEY_B,
		.write	= MFCL_ACCESS_KEY_B,
		.inc	= MFCL_ACCESS_NEVER,
		.dec	= MFCL_ACCESS_NEVER,
	},
	[3] = {
		.read	= MFCL_ACCESS_KEY_A | MFCL_ACCESS_KEY_B,
		.write	= MFCL_ACCESS_KEY_B,
		.inc	= MFCL_ACCESS_KEY_B,
		.dec	= MFCL_ACCESS_KEY_A | MFCL_ACCESS_KEY_B,
	},
	[4] = {
		.read	= MFCL_ACCESS_KEY_A | MFCL_ACCESS_KEY_B,
		.write	= MFCL_ACCESS_NEVER,
		.inc	= MFCL_ACCESS_NEVER,
		.dec	= MFCL_ACCESS_KEY_A | MFCL_ACCESS_KEY_B,
	},
	[6] = {
		.read	= MFCL_ACCESS_KEY_B,
		.write	= MFCL_ACCESS_KEY_B,
		.inc	= MFCL_ACCESS_NEVER,
		.dec	= MFCL_ACCESS_NEVER,
	},
	[5] = {
		.read	= MFCL_ACCESS_KEY_B,
		.write	= MFCL_ACCESS_NEVER,
		.inc	= MFCL_ACCESS_NEVER,
		.dec	= MFCL_ACCESS_NEVER,
	},
	[7] = {
		.read	= MFCL_ACCESS_NEVER,
		.write	= MFCL_ACCESS_NEVER,
		.inc	= MFCL_ACCESS_NEVER,
		.dec	= MFCL_ACCESS_NEVER,
	},
};

static const struct mfcl_access_exp_acc mfcl_exp_trailer[] = {
	[0] = {
		.key_a_rd	= MFCL_ACCESS_NEVER,
		.key_a_wr	= MFCL_ACCESS_KEY_A,
		.acc_rd		= MFCL_ACCESS_KEY_A,
		.acc_wr		= MFCL_ACCESS_NEVER,
		.key_b_rd	= MFCL_ACCESS_KEY_A,
		.key_b_wr	= MFCL_ACCESS_KEY_A,
	},
	[2] = {
		.key_a_rd	= MFCL_ACCESS_NEVER,
		.key_a_wr	= MFCL_ACCESS_NEVER,
		.acc_rd		= MFCL_ACCESS_KEY_A,
		.acc_wr		= MFCL_ACCESS_NEVER,
		.key_b_rd	= MFCL_ACCESS_KEY_A,
		.key_b_wr	= MFCL_ACCESS_NEVER,
	},
	[1] = {
		.key_a_rd	= MFCL_ACCESS_NEVER,
		.key_a_wr	= MFCL_ACCESS_KEY_B,
		.acc_rd		= MFCL_ACCESS_KEY_A | MFCL_ACCESS_KEY_B,
		.acc_wr		= MFCL_ACCESS_NEVER,
		.key_b_rd	= MFCL_ACCESS_NEVER,
		.key_b_wr	= MFCL_ACCESS_KEY_B,
	},
	[3] = {
		.key_a_rd	= MFCL_ACCESS_NEVER,
		.key_a_wr	= MFCL_ACCESS_NEVER,
		.acc_rd		= MFCL_ACCESS_KEY_A | MFCL_ACCESS_KEY_B,
		.acc_wr		= MFCL_ACCESS_NEVER,
		.key_b_rd	= MFCL_ACCESS_NEVER,
		.key_b_wr	= MFCL_ACCESS_NEVER,
	},
	[4] = {
		.key_a_rd	= MFCL_ACCESS_NEVER,
		.key_a_wr	= MFCL_ACCESS_KEY_A,
		.acc_rd		= MFCL_ACCESS_KEY_A,
		.acc_wr		= MFCL_ACCESS_KEY_A,
		.key_b_rd	= MFCL_ACCESS_KEY_A,
		.key_b_wr	= MFCL_ACCESS_KEY_A,
	},
	[6] = {
		.key_a_rd	= MFCL_ACCESS_NEVER,
		.key_a_wr	= MFCL_ACCESS_KEY_B,
		.acc_rd		= MFCL_ACCESS_KEY_A | MFCL_ACCESS_KEY_B,
		.acc_wr		= MFCL_ACCESS_KEY_B,
		.key_b_rd	= MFCL_ACCESS_NEVER,
		.key_b_wr	= MFCL_ACCESS_KEY_B,
	},
	[5] = {
		.key_a_rd	= MFCL_ACCESS_NEVER,
		.key_a_wr	= MFCL_ACCESS_NEVER,
		.acc_rd		= MFCL_ACCESS_KEY_A | MFCL_ACCESS_KEY_B,
		.acc_wr		= MFCL_ACCESS_KEY_B,
		.key_b_rd	= MFCL_ACCESS_NEVER,
		.key_b_wr	= MFCL_ACCESS_NEVER,
	},
	[7] = {
		.key_a_rd	= MFCL_ACCESS_NEVER,
		.key_a_wr	= MFCL_ACCESS_NEVER,
		.acc_rd		= MFCL_ACCESS_KEY_A | MFCL_ACCESS_KEY_B,
		.acc_wr		= MFCL_ACCESS_NEVER,
		.key_b_rd	= MFCL_ACCESS_NEVER,
		.key_b_wr	= MFCL_ACCESS_NEVER,
	},
};

const char *mfcl_access_str[] = {
	[MFCL_ACCESS_NEVER] = "NEVER",
	[MFCL_ACCESS_KEY_A] = "A",
	[MFCL_ACCESS_KEY_B] = "B",
	[MFCL_ACCESS_KEY_A|MFCL_ACCESS_KEY_B] = "A|B",
};

void mfcl_access_to_exp(struct mfcl_access_exp_sect *exp,
			const struct mfcl_access_sect *sect)
{
	int b;
	for (b = 0; b < 3; b++)
		exp->block[b] = mfcl_exp_data[sect->block[b]];
	exp->acc = mfcl_exp_trailer[sect->block[3]];
}

char *mfcl_access_exp_stringify(const struct mfcl_access_exp_block *exp)
{
	static char buf[80];

	sprintf(buf, "READ: %s, WRITE: %s, INC: %s, DEC: %s",
		mfcl_access_str[exp->read],
		mfcl_access_str[exp->write],
		mfcl_access_str[exp->inc],
		mfcl_access_str[exp->dec]);
	return buf;
}

char *mfcl_access_exp_acc_stringify(const struct mfcl_access_exp_acc *acc)
{
	static char buf[80];

	sprintf(buf, "KEY_A_RD: %s, KEY_A_WR: %s, ACC_RD: %s, ACC_WR: %s, "
		"KEY_B_RD: %s, KEY_B_WR: %s",
		mfcl_access_str[acc->key_a_rd],
		mfcl_access_str[acc->key_a_wr],
		mfcl_access_str[acc->acc_rd],
		mfcl_access_str[acc->acc_wr],
		mfcl_access_str[acc->key_b_rd],
		mfcl_access_str[acc->key_b_wr]);
	return buf;
}
