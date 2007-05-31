/* Python bindings for librfid 
 *  (C) 2007 by Kushal Das <kushal@openpcd.org>
 *
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>

#include <librfid/rfid.h>
#include <librfid/rfid_scan.h>
#include <librfid/rfid_reader.h>
#include <librfid/rfid_layer2.h>
#include <librfid/rfid_protocol.h>

#include <librfid/rfid_protocol_mifare_classic.h>
#include <librfid/rfid_protocol_mifare_ul.h>
#include <common.h>

#include <Python.h>
static PyObject *pyi_open(PyObject *self, PyObject *args);
static PyObject *pyi_close(PyObject *self, PyObject *args);
static PyObject *pyi_rfidscan(PyObject *self, PyObject *args);
static PyObject *pyi_rfidlayeropt(PyObject *self, PyObject *args);

static PyObject *pyi_Error;
struct rfid_reader_handle *rh;
struct rfid_layer2_handle *l2h;
struct rfid_protocol_handle *ph;

static PyMethodDef pyi_Methods[] = {
    {"open", pyi_open, METH_VARARGS,
        "This will initialise the RFID reader"},
    {"close", pyi_close, METH_VARARGS,
        "This will close the RFID reader"},
    {"scan", pyi_rfidscan, METH_VARARGS,
        "This will scan for any card"},
    {"get_id", pyi_rfidlayeropt, METH_VARARGS,
        "This will read the id of the card"},
    {NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC initpyrfid() {
    PyObject *m;

    m = Py_InitModule("pyrfid", pyi_Methods);
    pyi_Error = PyErr_NewException("pyrfid.error", NULL, NULL);
    Py_INCREF(pyi_Error);
    PyModule_AddObject(m, "error", pyi_Error);
    return;
}

static PyObject *pyi_open(PyObject *self, PyObject *args) {
    rfid_init();
    rh = rfid_reader_open(NULL, RFID_READER_OPENPCD);
    if (!rh)
	return Py_BuildValue("i", 1);
    else
	return Py_BuildValue("i", 0);
}

static PyObject *pyi_close(PyObject *self, PyObject *args) {
    rfid_reader_close(rh);
//     Py_INCREF(Py_None);
//     return Py_None;
     return Py_BuildValue("i", 0);
}

static PyObject *pyi_rfidscan(PyObject *self, PyObject *args) {
    int rc;
	rc = rfid_scan(rh, &l2h, &ph);
	return Py_BuildValue("i", rc);
}

static PyObject *pyi_rfidlayeropt(PyObject *self, PyObject *args) {
                unsigned char uid_buf[16];
		char card_id[16];
		unsigned int uid_len = sizeof(uid_buf);
		rfid_layer2_getopt(l2h, RFID_OPT_LAYER2_UID, &uid_buf,
				   &uid_len);
		strcpy(card_id,hexdump(uid_buf, uid_len));
		return Py_BuildValue("s", card_id);
}
