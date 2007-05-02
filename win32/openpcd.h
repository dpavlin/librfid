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

#ifndef __OPENPCD_H__
#define __OPENPCD_H__

#ifdef  __cplusplus
extern "C" {
#endif/*__cplusplus*/

#define EXPORT_CONVENTION __stdcall
#ifdef  BUILD_DLL
/* DLL export */
#define EXPORT __declspec(dllexport)
#else
/* EXE import */
#define EXPORT __declspec(dllimport)
#endif /*BUILD_DLL*/

#define PCDERROR_NONE			 0
#define PCDERROR_INVALID_PARAMETER	-1
#define PCDERROR_KEY_FORMAT		-2
#define PCDERROR_KEY_AUTH		-3
#define PCDERROR_NO_CARD_FOUND		-4
#define PCDERROR_LAYER2_INIT		-5
#define PCDERROR_LAYER2_OPEN		-6
#define PCDERROR_LAYER3_INIT		-7
#define PCDERROR_LAYER3_OPEN		-8
#define PCDERROR_SELECT			-9
#define PCDERROR_READ_FAILED		-10
#define PCDERROR_WRITE_FAILED		-11
#define PCDERROR_CLOSED			-12
#define PCDERROR_NO_READER		-13
#define PCDERROR_OUT_OF_MEMORY		-14
#define	PCDERROR_READER_VERSION		-15

#define PCDAUTH_KEY_LENGTH 6
#define PCDAUTH_KEYID_1A 0
#define PCDAUTH_KEYID_1B 1

typedef void* MIFARE_HANDLE;

/*************************************************************************/
/*                                                                       */
/* Six steps for reading/writing to MIFARE cards                         */
/*                                                                       */
/*************************************************************************/

/*  Step 1. open reader

    supply the address of your handle variable to retrieve a handle
    to the current reader.
 */
EXPORT int EXPORT_CONVENTION openpcd_open_reader(MIFARE_HANDLE *handle);

/*  Step 2. set MIFARE classic key

    if your key differs from the default Infineon key (6*0xFF), you can
    supply a different key here. The key size is PCDAUTH_KEY_LENGTH bytes.
    You can chose to set key_id to PCDAUTH_KEYID_1A or *_1B.
 */
EXPORT int EXPORT_CONVENTION openpcd_set_key(MIFARE_HANDLE handle,unsigned int key_id,const void* key);

/*  Step 3. select card
    
    start the anticollosion to select a card in the reader field - retry if
    it fails. Currently supports only on card in the readerv field.
 */
EXPORT int EXPORT_CONVENTION openpcd_select_card(MIFARE_HANDLE handle);

/*  Step 4. read/write card
    
    read, write from the selected card - specify the page and supply up to
    16 bytes of payload
 */
EXPORT int EXPORT_CONVENTION openpcd_read(MIFARE_HANDLE handle,int page, void* data, int len);
EXPORT int EXPORT_CONVENTION openpcd_write(MIFARE_HANDLE handle,int page,const void *data,int len);

/*  Step 5. deselect card when done
 */
EXPORT int EXPORT_CONVENTION openpcd_deselect_card(MIFARE_HANDLE handle);

/*  Step 6. close reader after deselected card
 */
EXPORT int EXPORT_CONVENTION openpcd_close_reader(MIFARE_HANDLE handle);


/*************************************************************************/
/*                                                                       */
/* Support functions                                                     */
/*                                                                       */
/*************************************************************************/

/*  openpcd_get_error_text:

    Used for converting the error code into a string
 */
EXPORT char* EXPORT_CONVENTION openpcd_get_error_text(int error);


/*  openpcd_get_card_id:

    Get the card id of a selected RFID card
 */
EXPORT int EXPORT_CONVENTION openpcd_get_card_id(MIFARE_HANDLE handle,unsigned int *uid);

/*  openpcd_get_api_version:

    Get the USB api version of the reader
 */
EXPORT int EXPORT_CONVENTION openpcd_get_api_version(MIFARE_HANDLE handle,unsigned int *version);

/*  openpcd_reset_reader:

    Reset the attached reader
 */
EXPORT int EXPORT_CONVENTION openpcd_reset_reader(MIFARE_HANDLE handle);

/*  openpcd_get_environment

    Store the given data to the nonvolatile reader flash
    Returns read data count at index or error code
 */
EXPORT int EXPORT_CONVENTION openpcd_get_environment(
    MIFARE_HANDLE handle,
    unsigned char count,
    unsigned char* data    
);
    
/*  openpcd_set_environment

    Read data from nonvolatile reader flash
    Returns written data count at index or error code
 */
EXPORT int EXPORT_CONVENTION openpcd_set_environment(
    MIFARE_HANDLE handle,
    unsigned char count,
    const unsigned char* data    
);

#ifdef  __cplusplus
}
#endif/*__cplusplus*/
#endif/*__OPENPCD_H__*/
