static unsigned int fsdi_table[] = { 16, 24, 32, 40, 48, 64, 96, 128, 256 };

int iso14443_fsdi_to_fsd(unsigned int *fsd, unsigned char fsdi)
{
	/* ISO 14443-4:2000(E) Section 5.1. */
	if (fsdi > sizeof(fsdi_table)/sizeof(unsigned int))
		return -1;

	*fsd = fsdi_table[fsdi];
	return 0;
}

int iso14443_fsd_to_fsdi(unsigned char *fsdi, unsigned int fsd)
{
	int i;

	for (i = 0; i < sizeof(fsdi_table)/sizeof(unsigned int); i++) {
		if (fsdi_table[i] == fsd) {
			*fsdi = i;
			return 0;
		}
	}

	return -1;
}

/* calculate the fsd that is equal or the closest smaller value
 * that can be coded as fsd */
unsigned int iso14443_fsd_approx(unsigned int fsd)
{
	unsigned int tbl_size = sizeof(fsdi_table)/sizeof(unsigned int);
	int i;

	for (i = tbl_size-1; i >= 0; i--) {
		if (fsdi_table[i] <= fsd)
			return fsdi_table[i];
	}
	/* not reached: return smallest possible FSD */
	return fsdi_table[0];
}

