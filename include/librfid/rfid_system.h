/* system / environment specific defines */

/* build for openpcd firmware */
//#define LIBRFID_FIRMWARE

/* build without dynamic allocations */
//#define LIBRFID_STATIC

#ifdef __LIBRFID__

#ifndef LIBRFID_STATIC
/* If we're not doing a firmware compile, then we just use the regular
 * malloc()/free() functions as expected */

#define malloc_asic_handle(x)	malloc(x)
#define free_asic_handle(x)	free(x)

#define malloc_layer2_handle(x)	malloc(x)
#define free_layer2_handle(x)	free(x)

#define malloc_protocol_handle(x)	malloc(x)
#define free_protocol_handle(x)	free(x)

#define malloc_rat_handle(x)	malloc(x)
#define free_rat_handle(x)	free(x)

#define malloc_reader_handle(x)	malloc(x)
#define free_reader_handle(x)	free(x)

#else
/* If we're actually doing a firmware compile, then we use pre-allocated
 * handles in order to avoid dynamic memory allocation. */

#define EMPTY_STATEMENT	do {} while(0)
extern struct rfid_asic_handle rfid_ah;
#define malloc_asic_handle(x)	&rfid_ah
#define free_asic_handle(x)	EMPTY_STATEMENT

extern struct rfid_layer2_handle rfid_l2h;
#define malloc_layer2_handle(x)	&rfid_l2h
#define free_layer2_handle(x)	EMPTY_STATEMENT

extern struct rfid_protocol_handle rfid_ph;
#define malloc_protocol_handle(x)	&rfid_ph
#define free_protocol_handle(x)		EMPTY_STATEMENT

extern struct rfid_asic_transport_handle rfid_ath;
#define malloc_rat_handle(x)	&rfid_ath
#define free_rat_handle(x)	EMPTY_STATEMENT

extern struct rfid_reader_handle rfid_rh;
#define malloc_reader_handle(x)	&rfid_rh
#define free_reader_handle(x)	EMPTY_STATEMENT

#endif /* LIBRFID_STATIC */

#endif /* __LIBRFID__ */
