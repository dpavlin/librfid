
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

