#ifndef _RFID_PROTOCOL_TCL_H
#define _RFID_PROTOCOL_TCL_H

enum rfid_proto_tcl_opt {
	RFID_OPT_P_TCL_ATS	=	0x00010001,
	RFID_OPT_P_TCL_ATS_LEN	=	0x00010002,
};

#ifdef __LIBRFID__

enum tcl_transport_rate {
	TCL_RATE_106	= 0x01,
	TCL_RATE_212	= 0x02,
	TCL_RATE_424	= 0x04,
	TCL_RATE_848	= 0x08,
};

enum tcl_transport_transceive_flags {
	TCL_TRANSP_F_TX_CRC	= 0x01,	/* transport adds TX CRC */
	TCL_TRANSP_F_RX_CRC	= 0x02, 
};

struct tcl_handle {
	/* derived from ats */
	unsigned char *historical_bytes; /* points into ats */
	unsigned int historical_len;

	unsigned int fsc;	/* max frame size accepted by card */
	unsigned int fsd;	/* max frame size accepted by reader */
	unsigned int fwt;	/* frame waiting time (in usec)*/
	unsigned char ta;	/* divisor information */
	unsigned char sfgt;	/* start-up frame guard time (in usec) */

	/* otherwise determined */
	unsigned int cid;	/* Card ID */
	unsigned int nad;	/* Node Address */

	unsigned int flags;
	unsigned int state;	/* protocol state */

	unsigned int toggle;	/* send toggle with next frame */

	unsigned int ats_len;
	unsigned char ats[256];	/* ATS cannot be bigger than FSD-2 bytes,
				   according to ISO 14443-4 5.2.2 */
};

enum tcl_handle_flags {
	TCL_HANDLE_F_NAD_SUPPORTED 	= 0x0001,
	TCL_HANDLE_F_CID_SUPPORTED 	= 0x0002,
	TCL_HANDLE_F_NAD_USED		= 0x0010,
	TCL_HANDLE_F_CID_USED		= 0x0020,
};


enum tcl_pcb_bits {
	TCL_PCB_CID_FOLLOWING		= 0x08,
	TCL_PCB_NAD_FOLLOWING		= 0x04,
};

enum tcl_pcd_state {
	TCL_STATE_NONE = 0x00,
	TCL_STATE_INITIAL,
	TCL_STATE_RATS_SENT,		/* waiting for ATS */
	TCL_STATE_ATS_RCVD,		/* ATS received */
	TCL_STATE_PPS_SENT,		/* waiting for PPS response */
	TCL_STATE_ESTABLISHED,		/* xchg transparent data */
	TCL_STATE_DESELECT_SENT,	/* waiting for DESELECT response */
	TCL_STATE_DESELECTED,		/* card deselected or HLTA'd */
};

extern const struct rfid_protocol rfid_protocol_tcl;

#endif /* __LIBRFID__ */

#endif
