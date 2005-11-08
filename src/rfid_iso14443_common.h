#ifndef __RFID_ISO14443_COMMON_H
#define __RFID_ISO14443_COMMON_H

int iso14443_fsdi_to_fsd(unsigned int *fsd, unsigned char fsdi);
int iso14443_fsd_to_fsdi(unsigned char *fsdi, unsigned int fsd);
unsigned int iso14443_fsd_approx(unsigned int fsd);

#define ISO14443_FREQ_CARRIER		13560000
#define ISO14443_FREQ_SUBCARRIER	(ISO14443_FREQ_CARRIER/16)

#endif
