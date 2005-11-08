#ifndef _RFID_ASIC_H
#define _RFID_ASIC_H

enum rfid_frametype;

#include <librfid/rfid_asic_rc632.h>

struct rfid_asic_transport {
	char *name;
	union {
		struct rfid_asic_rc632_transport rc632;
	} priv;
};

struct rfid_asic_transport_handle {
	void *data;		/* handle to stuff like even lower layers */

	struct rfid_asic_transport *rat;
};


struct rfid_asic_handle {
	struct rfid_asic_transport_handle *rath;
	unsigned int fc;
	unsigned int mtu;
	unsigned int mru;

	union {
		struct rfid_asic_rc632_handle rc632;
		//struct rfid_asic_rc531_handle rc531;
	} priv;
	struct rfid_asic *asic;
};


struct rfid_asic {
	char *name;
	unsigned int fc;		/* carrier frequency */
	union {
		struct rfid_asic_rc632 rc632;
		//struct rfid_asic_rc531 rc531;
	} priv;
};

#endif /* _RFID_ASIC_H */
