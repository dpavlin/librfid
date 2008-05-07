/*************************************************************************/
/*                                                                       */
/* Mifare support for accessing RFID cards with OpenPCD RFID reader      */
/* in WIN32 - see http://www.openpcd.org                                 */
/*                                                                       */
/* Copyright (C) 2007 Milosch Meriac <meriac@bitmanufaktur.de>           */
/*                                                                       */
/* Redistribution and use in source and binary forms, with or without    */
/* modification, are permitted provided that the following conditions are*/
/* met:                                                                  */
/*                                                                       */
/* Redistributions of source code must retain the above copyright notice,*/
/* this list of conditions and the following disclaimer.                 */
/* Redistributions in binary form must reproduce the above copyright     */
/* notice, this list of conditions and the following disclaimer in the   */
/* documentation and/or other materials provided with the distribution.  */
/*                                                                       */
/* The name of the author may not be used to endorse or promote products */
/* derived from this software without specific prior written permission. */
/*                                                                       */
/* THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR  */
/* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED        */
/* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE*/
/* DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,    */
/* INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES    */
/* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR    */
/* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)    */
/* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,   */
/* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING */
/* IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE    */
/* POSSIBILITY OF SUCH DAMAGE.                                           */
/*                                                                       */
/*************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <librfid/rfid.h>
#include <librfid/rfid_scan.h>
#include <librfid/rfid_reader.h>
#include <librfid/rfid_protocol.h>
#include <librfid/rfid_protocol_mifare_classic.h>

#define BUILD_DLL
#include "openpcd.h"

#define LIBMIFARE_MAGIC 0xDEADBEEF

struct openpcd_state
{
    unsigned int magic;    
    unsigned int cl_auth;
    struct rfid_reader_handle *rh;
    struct rfid_layer2_handle *l2h;
    struct rfid_protocol_handle *ph;
    unsigned char key[MIFARE_CL_KEY_LEN];
    unsigned int uid;
} openpcd_state;

int openpcd_cl_auth(struct openpcd_state* state ,int page)
{
    int rc;

    if(!state || page<=0 || page>MIFARE_CL_PAGE_MAX )
	return PCDERROR_INVALID_PARAMETER;
	
    if(!state->ph)
	return PCDERROR_CLOSED;

    rc = mfcl_set_key(state->ph, state->key);
    if (rc < 0)
	return PCDERROR_KEY_FORMAT;
	
    rc = mfcl_auth(state->ph, state->cl_auth, page);

    return rc<0 ? PCDERROR_KEY_AUTH : PCDERROR_NONE;
}

void Sleep(unsigned int ms ) {
    usleep(ms*1000);
}

EXPORT int EXPORT_CONVENTION openpcd_set_key(MIFARE_HANDLE handle,unsigned int key_id,const void* key)
{
    struct openpcd_state *state;

    if(!handle)
	return PCDERROR_INVALID_PARAMETER;
    state=(struct openpcd_state*)handle;
    
    switch(key_id)
    {
	case PCDAUTH_KEYID_1A:
	    state->cl_auth=RFID_CMD_MIFARE_AUTH1A;
	    break;
	case PCDAUTH_KEYID_1B:
	    state->cl_auth=RFID_CMD_MIFARE_AUTH1B;
	    break;
	default:
	    return PCDERROR_INVALID_PARAMETER;
    }
    
    memcpy(state->key,key,MIFARE_CL_KEY_LEN);
    
    return PCDERROR_NONE;
}

EXPORT int EXPORT_CONVENTION openpcd_select_card(MIFARE_HANDLE handle)
{
    int res;
    struct openpcd_state *state;
        
    if(!handle)
	return PCDERROR_INVALID_PARAMETER;
    state=(struct openpcd_state*)handle;
    
    state->l2h = rfid_layer2_init(state->rh,RFID_LAYER2_ISO14443A);
    if(!state->l2h)
	res=PCDERROR_LAYER2_INIT;
    else
    {        
	if( rfid_layer2_open(state->l2h)>=0 ) 
	{
    	    state->ph = rfid_protocol_init(state->l2h,RFID_PROTOCOL_MIFARE_CLASSIC);
	
	    if(state->ph)
    	    {
		if(rfid_protocol_open(state->ph)>=0)
	    	    return PCDERROR_NONE;
		
		rfid_protocol_fini(state->ph);
		state->ph=NULL;
	
		res=PCDERROR_LAYER3_OPEN;
	    }
	    else
		res=PCDERROR_LAYER3_INIT;

	    rfid_layer2_close(state->l2h);	
        }
	else
    	    res=PCDERROR_LAYER2_OPEN;
    }
    
    rfid_layer2_fini(state->l2h);
    state->l2h=NULL;
    
    return res;	
}

EXPORT int EXPORT_CONVENTION openpcd_deselect_card(MIFARE_HANDLE handle)
{
    struct openpcd_state *state;
        
    if(!handle)
	return PCDERROR_INVALID_PARAMETER;
    state=(struct openpcd_state*)handle;
    
    if(state->ph)
    {
	rfid_protocol_close(state->ph);
	rfid_protocol_fini(state->ph);
	rfid_layer2_close(state->l2h);
	rfid_layer2_fini(state->l2h);

	state->ph=NULL;
	state->l2h=NULL;
	state->uid=0;
	
	return PCDERROR_NONE;
    }    
    else
	return PCDERROR_CLOSED;
}

EXPORT int EXPORT_CONVENTION openpcd_get_card_id(MIFARE_HANDLE handle,unsigned int *uid)
{
    unsigned int uid_len;
    struct openpcd_state *state;
        
    if(!handle || !uid)
	return PCDERROR_INVALID_PARAMETER;
    state=(struct openpcd_state*)handle;
    
    if(state->ph)
    {
	uid_len=sizeof(*uid);
	if(rfid_layer2_getopt(state->l2h,RFID_OPT_LAYER2_UID,uid,&uid_len))
	    return PCDERROR_INVALID_PARAMETER;
	else
	    return uid_len==4 ? PCDERROR_NONE:PCDERROR_READ_FAILED;
    }
    else
	return PCDERROR_CLOSED;    
}

EXPORT int EXPORT_CONVENTION openpcd_open_reader(MIFARE_HANDLE *handle)
{
    struct rfid_reader_handle *rh;
    struct openpcd_state *state;
    
    if(!handle)
	return PCDERROR_INVALID_PARAMETER;
	
    rh = rfid_reader_open(NULL, RFID_READER_OPENPCD);
    if(!rh)
	return PCDERROR_NO_READER;
    
    state=(struct openpcd_state*)malloc(sizeof(*state));
    if(state)
    {
	memset(state,0,sizeof(*state));
	state->magic=LIBMIFARE_MAGIC;
	state->rh=rh;
	state->cl_auth=RFID_CMD_MIFARE_AUTH1A;
	memset(state->key,0xFF,sizeof(state->key));
	
	// do initial reset
	openpcd_reset_reader((MIFARE_HANDLE)state);
	Sleep(1500);
	// reopen
        state->rh = rfid_reader_open(NULL, RFID_READER_OPENPCD);
	
	*handle=(MIFARE_HANDLE)state;
	
	return PCDERROR_NONE;
    }
    else
    {	
	rfid_reader_close(rh);
	return PCDERROR_OUT_OF_MEMORY;
    }
}

EXPORT int EXPORT_CONVENTION openpcd_close_reader(MIFARE_HANDLE handle)
{
    struct openpcd_state *state;
    
    if(!handle)
	return PCDERROR_INVALID_PARAMETER;
    state=(struct openpcd_state*)handle;

    openpcd_deselect_card(handle);

    openpcd_reset_reader(handle);
    Sleep(500);

    state->magic=0;
    rfid_reader_close(state->rh);
    free(state);    
    
    return PCDERROR_NONE;
}

EXPORT int EXPORT_CONVENTION openpcd_read(MIFARE_HANDLE handle,int page, void* data, int len)
{
    int res;
    unsigned int count;
    unsigned char buf[MIFARE_CL_PAGE_SIZE];
    struct openpcd_state *state;        
    
    if( !handle || !buf || page<0 || page>MIFARE_CL_PAGE_MAX || len<=0 || len>sizeof(buf))
	return PCDERROR_INVALID_PARAMETER;
	
    state=(struct openpcd_state*)handle;
    if ( (res=openpcd_cl_auth(state,page)) < 0)
	return res;
	
    count = sizeof(buf);
    res = rfid_protocol_read(state->ph, page, buf, &count);    
    if(res>=0)
	memcpy(data,buf,len);

    if ( res<0 )
	return PCDERROR_READ_FAILED;
    else
	return count;
}

EXPORT int EXPORT_CONVENTION openpcd_write(MIFARE_HANDLE handle,int page,const void *data,int len)
{
    int res;
    unsigned char buf[16];
    struct openpcd_state *state;

    if( !handle || !buf || page<0 || page>MIFARE_CL_PAGE_MAX || len<=0 || len>sizeof(buf))
	return PCDERROR_INVALID_PARAMETER;
	
    state=(struct openpcd_state*)handle;
    if ( (res=openpcd_cl_auth(state,page)) < 0)
    	return res;

    memcpy(buf,data,len);
    memset(&buf[len],0,sizeof(buf)-len);
    
    res = rfid_protocol_write(state->ph, page, buf, sizeof(buf));
    
    return (res<0 && res!=-101) ? PCDERROR_WRITE_FAILED : len;
}


EXPORT int EXPORT_CONVENTION openpcd_reset_reader(MIFARE_HANDLE handle)
{
    struct openpcd_state *state;

    if( !handle )
	return PCDERROR_INVALID_PARAMETER;
    state=(struct openpcd_state*)handle;
	
    return (state->rh->reader->reset(state->rh)<0) ? PCDERROR_WRITE_FAILED : PCDERROR_NONE;
}


EXPORT char* EXPORT_CONVENTION openpcd_get_error_text(int error)
{
    const static char* msg[]={
	"PCDERROR_NONE",		//  0
	"PCDERROR_INVALID_PARAMETER",	// -1
	"PCDERROR_KEY_FORMAT",		// -2
	"PCDERROR_KEY_AUTH",		// -3
	"PCDERROR_NO_CARD_FOUND",	// -4
	"PCDERROR_LAYER2_INIT",		// -5
	"PCDERROR_LAYER2_OPEN",		// -6
	"PCDERROR_LAYER3_INIT",		// -7
	"PCDERROR_LAYER3_OPEN",		// -8
	"PCDERROR_SELECT",		// -9
	"PCDERROR_READ_FAILED",		// -10
	"PCDERROR_WRITE_FAILED",	// -11
	"PCDERROR_CLOSED",		// -12
	"PCDERROR_NO_READER",		// -13
	"PCDERROR_OUT_OF_MEMORY",	// -14
	"PCDERROR_READER_VERSION"	// -15
    };
    const int count=sizeof(msg)/sizeof(msg[0]);
    
    if(error>0)
	error=0;
    else
	error=-error;
		
    return (error>=count) ? "PCDERROR_UNKNOWN" : (char*)msg[error];
}
