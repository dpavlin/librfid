#ifndef _RFID_MIFARE_ACCESS_H
#define _RFID_MIFARE_ACCESS_H

struct mfcl_access_sect {
	u_int8_t block[4];
};

int mfcl_compile_access(u_int8_t *encoded,
		        const struct mfcl_access_sect *ac);
int mfcl_parse_access(struct mfcl_access_sect *ac, u_int8_t *encoded);


enum mfcl_access_exp_data {
	MFCL_ACCESS_NEVER 	= 0,
	MFCL_ACCESS_KEY_A	= 1,
	MFCL_ACCESS_KEY_B	= 2,
};

struct mfcl_access_exp_block {
#ifndef RFID_BIG_ENDIAN_BITFIELD
	u_int8_t read:2,
		 write:2,
		 inc:2,
		 dec:2;
#else
	u_int8_t dec:2,
		 inc:2,
		 write:2,
		 read:2;
#endif
} __attribute__ ((packed));

struct mfcl_access_exp_acc {
#ifndef RFID_BIG_ENDIAN_BITFIELD
	u_int8_t key_a_rd:2,
		  key_a_wr:2,
		  acc_rd:2,
		  acc_wr:2;
	u_int8_t key_b_rd:2,
		  key_b_wr:2,
		  reserved:4;
#else
	u_int8_t acc_wr:2,
		  acc_rd:2,
		  key_a_wr:2,
		  key_a_rd:2;
	u_int8_t reserved:4,
		  key_b_wr:2,
		  key_b_rd:2;
#endif
} __attribute__ ((packed));


struct mfcl_access_exp_sect {
	struct mfcl_access_exp_block block[3];
	struct mfcl_access_exp_acc acc;
};

void mfcl_access_to_exp(struct mfcl_access_exp_sect *exp,
			const struct mfcl_access_sect *sect);

char *mfcl_access_exp_stringify(const struct mfcl_access_exp_block *exp);
char *mfcl_access_exp_acc_stringify(const struct mfcl_access_exp_acc *acc);
#endif
