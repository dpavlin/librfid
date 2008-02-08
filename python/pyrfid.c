/* Python bindings for librfid 
 *  (C) 2007-2008 by Kushal Das <kushal@openpcd.org>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
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
/*#include <common.h>*/
#include "openpcd.h"
#include <Python.h>
static PyObject *pyi_open(PyObject *self, PyObject *args);
static PyObject *pyi_close(PyObject *self, PyObject *args);
static PyObject *pyi_select_card(PyObject *self, PyObject *args);
static PyObject *pyi_deselect_card(PyObject *self, PyObject *args);
static PyObject *pyi_get_card_id(PyObject *self, PyObject *args);
static PyObject *pyi_openpcd_read(PyObject *self, PyObject *args);
static PyObject *pyi_openpcd_write(PyObject *self, PyObject *args);
static PyObject *pyi_set_key(PyObject *self, PyObject *args);

static PyObject *pyi_Error;
MIFARE_HANDLE handle;

static PyMethodDef pyi_Methods[] = {
    {"open_reader", pyi_open, METH_VARARGS,
        "This will initialise the RFID reader"},
    {"close_reader", pyi_close, METH_VARARGS,
        "This will close the RFID reader"},
    {"select_card", pyi_select_card, METH_VARARGS,
        "This will select any card"},
    {"read",pyi_openpcd_read, METH_VARARGS,
        "This will read the card with given page number"},
    {"write",pyi_openpcd_write, METH_VARARGS,
        "This will write the card with given page number"},
    {"set_key",pyi_set_key, METH_VARARGS,
        "This will set the key with given key"},
    {"deselect_card", pyi_deselect_card, METH_VARARGS,
        "This will deselect any card"},
    {"get_card_id", pyi_get_card_id, METH_VARARGS,
        "This will get the card id"},
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
        return Py_BuildValue("i",openpcd_open_reader(&handle));
}

static PyObject *pyi_close(PyObject *self, PyObject *args) {
        return Py_BuildValue("i", openpcd_close_reader(handle));
}

static PyObject *pyi_select_card(PyObject *self, PyObject *args) {
        return Py_BuildValue("i", openpcd_select_card(handle));
}

static PyObject *pyi_deselect_card(PyObject *self, PyObject *args) {
        return Py_BuildValue("i", openpcd_deselect_card(handle));
}

static PyObject *pyi_openpcd_read(PyObject *self, PyObject *args) {
        int ok, page;
        char s[16];
        ok = PyArg_ParseTuple(args, "i", &page);
        openpcd_read(handle, page, s, 16);
        return Py_BuildValue("s", s);
}


static PyObject *pyi_openpcd_write(PyObject *self, PyObject *args) {
        int ok, page, result;
        char *s = "indianindianindi";
        ok = PyArg_ParseTuple(args, "is", &page, &s);
        result = openpcd_write(handle, page, s, 16);
        return Py_BuildValue("i", result);
}

static PyObject *pyi_set_key(PyObject *self, PyObject *args) {
        int ok, key, result;
        char *s = "keykeykey";
        ok = PyArg_ParseTuple(args, "is", &key, &s);
        result = openpcd_set_key(handle, key, s);
        return Py_BuildValue("i", result);
}

static PyObject *pyi_get_card_id(PyObject *self, PyObject *args) {
	unsigned int uid;
	int result;
	result = openpcd_get_card_id(handle, &uid);
        if (result == 0)
		return Py_BuildValue("I", uid);
	else
		return Py_BuildValue("i", result);
}

