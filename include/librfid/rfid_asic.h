#ifndef _RFID_ASIC_H
#define _RFID_ASIC_H

enum rfid_frametype;

#include <librfid/rfid_asic_rc632.h>

/* a low-level transport, over which the ASIC layer can talk to its ASIC */
struct rfid_asic_transport {
	char *name;
	union {
		struct rfid_asic_rc632_transport rc632;
	} priv;
};

/* An instance handle to 'struct rfid_asic_transport' */
struct rfid_asic_transport_handle {
	void *data;		/* handle to stuff like even lower layers */

	const struct rfid_asic_transport *rat;
};

/* Some implementation-specific data.  A reader will specify one of these for
 * ASIC-specific parameters such as e.g. RC632 mod conductance */

struct rfid_asic_implementation {
	union {
		struct rfid_asic_rc632_impl rc632;
	} priv;
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
