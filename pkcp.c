#include <Python.h>
#include "structmember.h"
#include "kcp/ikcp.h"

//#define PKCP_DEBUG                  1

typedef struct {
    PyObject_HEAD
    int conv;
    ikcpcb *kcp;
    PyObject *output_callback;
    PyObject *udata;
    int minrto;
    int fastresend;
    uintptr_t current_mesc;
    int MAX_RECV_SIZE;
} pkcp_t;

static uintptr_t current_msec_update() {
    struct timeval tv;
    time_t sec;
    int32_t msec;

    gettimeofday(&tv, NULL);
    sec = tv.tv_sec;
    msec = tv.tv_usec / 1000;

    return (uintptr_t) sec * 1000 + msec;
}

static int output_callback(const char *buf, int len, ikcpcb *kcp, void *user) {
#ifdef PKCP_DEBUG
    printf("output_callback\n");
#endif
    pkcp_t *self = (pkcp_t *) user;
    PyObject *py_args = NULL, *py_result = NULL;
    py_args = Py_BuildValue("(y#,O)", buf, len, self->udata);
    py_result = PyObject_CallObject(self->output_callback, py_args);
    Py_DECREF(py_args);
    if (py_result != NULL) {
        Py_DECREF(py_result);
    }
#ifdef PKCP_DEBUG
    printf("output_callback end\n");
#endif
    return 0;
}

static int pkcp_traverse(pkcp_t *self, visitproc visit, void *arg) {
#ifdef PKCP_DEBUG
    printf("pkcp_traverse\n");
#endif
    Py_VISIT(self->output_callback);
    Py_VISIT(self->udata);
#ifdef PKCP_DEBUG
    printf("pkcp_traverse end\n");
#endif
    return 0;
}


static int pkcp_clear(pkcp_t *self) {
#ifdef PKCP_DEBUG
    printf("pkcp_clear\n");
#endif
    Py_XDECREF(self->output_callback);         /* Add a reference to new callback */
    Py_XDECREF(self->udata);
    ikcp_release(self->kcp);
    return 0;
}

static void pkcp_dealloc(pkcp_t *self) {
#ifdef PKCP_DEBUG
    printf("pkcp_dealloc\n");
#endif
    pkcp_clear(self);
    Py_TYPE(self)->tp_free((PyObject *) self);
}

/**
 * ikcp create
 */
static PyObject *pkcp_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    pkcp_t *self = (pkcp_t *) type->tp_alloc(type, 0);
    if (self != NULL) {
        // default
        self->conv = 100;
        self->MAX_RECV_SIZE = 4096;
    }

    return (PyObject *) self;
}

static int pkcp_init(pkcp_t *self, PyObject *args, PyObject *keywds) {
    PyObject *callback_fn = NULL, *udata = NULL;
    static char *kwlist[] = {"conv", "output", "udata", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywds, "iO|O", kwlist, &self->conv, &callback_fn, &udata)) {
        return -1;
    }
    if (!PyCallable_Check(callback_fn)) {
        PyErr_SetString(PyExc_TypeError, "parameter output must be callable, fn(buffer,udata)");
        return -1;
    }
    Py_XINCREF(callback_fn);         /* Add a reference to new callback */
    Py_XINCREF(udata);
    self->output_callback = callback_fn;       /* Remember new callback */
    self->udata = udata;

    ikcpcb *kcp = ikcp_create((IUINT32) self->conv, self);
    kcp->output = output_callback;
    self->kcp = kcp;
    self->current_mesc = current_msec_update();
    return 0;
}


static PyObject *pkcp_wndsize(pkcp_t *self, PyObject *args, PyObject *keywds) {
    PyObject *sndwnd = NULL, *rcvwnd = NULL;
    if (!PyArg_ParseTuple(args, "|ii", &sndwnd, &rcvwnd)) {
        Py_RETURN_NONE;
    }
    ikcp_wndsize(self->kcp, (int) sndwnd, (int) rcvwnd);
    Py_RETURN_NONE;
}

static PyObject *pkcp_nodelay(pkcp_t *self, PyObject *args, PyObject *keywds) {
    PyObject *nodelay = NULL, *interval = NULL, *resend = NULL, *nc = NULL;
    if (!PyArg_ParseTuple(args, "iiii", &nodelay, &interval, &resend, &nc)) {
        PyErr_SetString(PyExc_ValueError, "args error, nodelay(nodelay,interval,resend,nc)");
        Py_RETURN_NONE;
    }
    ikcp_nodelay(self->kcp, (int) nodelay, (int) interval, (int) resend, (int) nc);
    Py_RETURN_NONE;
}

static PyObject *pkcp_input(pkcp_t *self, PyObject *args, PyObject *keywds) {
#ifdef PKCP_DEBUG
    printf("pkcp_input\n");
#endif
    char *data;
    Py_ssize_t size;
    if (!PyArg_ParseTuple(args, "y#", &data, &size)) {
        printf("input error\n");
        Py_RETURN_NONE;
    }
    ikcp_input(self->kcp, data, (long) size);
#ifdef PKCP_DEBUG
    printf("pkcp_input end\n");
#endif
    Py_RETURN_NONE;
}

static PyObject *pkcp_update(pkcp_t *self, PyObject *args, PyObject *keywds) {
#ifdef PKCP_DEBUG
    printf("pkcp_update\n");
#endif

    ikcp_update(self->kcp, (IUINT32) (current_msec_update() - self->current_mesc));
//    Py_END_ALLOW_THREADS
#ifdef PKCP_DEBUG
    printf("pkcp_update end\n");
#endif
    Py_RETURN_NONE;
}

static PyObject *pkcp_check(pkcp_t *self, PyObject *args, PyObject *keywds) {
    uint32_t millisec = ikcp_check(self->kcp, (IUINT32) (current_msec_update() - self->current_mesc));
    return PyLong_FromLong(millisec);
}

static PyObject *pkcp_recv(pkcp_t *self, PyObject *args, PyObject *keywds) {
#ifdef PKCP_DEBUG
    printf("pkcp_recv\n");
#endif
    char buffer[self->MAX_RECV_SIZE];
    int count = ikcp_recv(self->kcp, buffer, self->MAX_RECV_SIZE);
    if (count < 1) {
        Py_RETURN_NONE;
    }
#ifdef PKCP_DEBUG
    printf("pkcp_recv end\n");
#endif
    // build bytes
    return Py_BuildValue("s#", buffer, count);
}

static PyObject *pkcp_send(pkcp_t *self, PyObject *args, PyObject *keywds) {
#ifdef PKCP_DEBUG
    printf("pkcp_send\n");
#endif
    char *data;
    Py_ssize_t size;
    if (!PyArg_ParseTuple(args, "s#", &data, &size)) {
        Py_RETURN_NONE;
    }
    ikcp_send(self->kcp, data, (int) size);
#ifdef PKCP_DEBUG
    printf("pkcp_send end\n");
#endif
    Py_RETURN_NONE;
}


static PyMethodDef pkcp_methods[] = {
        {"nodelay", (PyCFunction) pkcp_nodelay, METH_VARARGS, "ikcp_nodelay"},
        {"wndsize", (PyCFunction) pkcp_wndsize, METH_VARARGS, "ikcp_wndsize"},
        {"input",   (PyCFunction) pkcp_input,   METH_VARARGS, "ikcp_input"},
        {"update",  (PyCFunction) pkcp_update,  METH_VARARGS, "ikcp_update"},
        {"check",   (PyCFunction) pkcp_check,   METH_VARARGS, "ikcp_recv"},
        {"recv",    (PyCFunction) pkcp_recv,    METH_VARARGS, "ikcp_recv"},
        {"send",    (PyCFunction) pkcp_send,    METH_VARARGS, "ikcp_recv"},
        {NULL}  /* Sentinel */
};

/////////////// getseters ///////////////

static PyObject *pkcp_get_minrto(pkcp_t *self, void *closure) {
    return PyLong_FromLong(self->minrto);
}

static int pkcp_set_minrto(pkcp_t *self, PyObject *value, void *closure) {
    if (value == NULL) {
        PyErr_SetString(PyExc_AttributeError, "Cannot set minrto attribute");
        return -1;
    }
    if (!PyLong_Check(value)) {
        PyErr_SetString(PyExc_TypeError,
                        "The minrto attribute value must be a int");
        return -1;
    }
    self->minrto = (int) PyLong_AsLong(value);
    self->kcp->rx_minrto = self->minrto;
    return 0;
}

static PyObject *pkcp_get_fastresend(pkcp_t *self, void *closure) {
    return PyLong_FromLong(self->fastresend);
}

static int pkcp_set_fastresend(pkcp_t *self, PyObject *value, void *closure) {
    if (value == NULL) {
        PyErr_SetString(PyExc_AttributeError, "Cannot set fastresend attribute");
        return -1;
    }
    if (!PyLong_Check(value)) {
        PyErr_SetString(PyExc_TypeError,
                        "The fastresend attribute value must be a int");
        return -1;
    }
    self->minrto = (int) PyLong_AsLong(value);
    self->kcp->fastresend = self->fastresend;
    return 0;
}

static PyGetSetDef pkcp_getseters[] = {
        {"minrto",     (getter) pkcp_get_minrto,     (setter) pkcp_set_minrto,
                "kcp rx_minrto",  NULL},
        {"fastresend", (getter) pkcp_get_fastresend, (setter) pkcp_set_fastresend,
                "kcp fastresend", NULL},
        {NULL}  /* Sentinel */
};

/////////////// members ///////////////
static PyMemberDef pkcp_members[] = {
//        {"minrto",           T_INT,       offsetof(pkcp_t, minrto),          READONLY, "first name"},
        {"MAX_RECV_SIZE",   T_INT,       offsetof(pkcp_t, MAX_RECV_SIZE), 0,          "attr MAX_RECV_SIZE"},
        {"output_callback", T_OBJECT_EX, offsetof(pkcp_t, output_callback), READONLY, "attr output_callback"},
        {"conv",            T_INT,       offsetof(pkcp_t, conv),            READONLY, "attr conv"},
        {"udata",           T_OBJECT_EX, offsetof(pkcp_t, udata),           READONLY, "attr udata"},
        {NULL}  /* Sentinel */
};


static PyTypeObject pkcpType = {
        PyVarObject_HEAD_INIT(NULL, 0)
        "pkcp.pkcp",                        /* tp_name */
        sizeof(pkcp_t),                     /* tp_basicsize */
        0,                                  /* tp_itemsize */
        (destructor) pkcp_dealloc,          /* tp_dealloc */
        0,                                  /* tp_print */
        0,                                  /* tp_getattr */
        0,                                  /* tp_setattr */
        0,                                  /* tp_reserved */
        0,                                  /* tp_repr */
        0,                                  /* tp_as_number */
        0,                                  /* tp_as_sequence */
        0,                                  /* tp_as_mapping */
        0,                                  /* tp_hash  */
        0,                                  /* tp_call */
        0,                                  /* tp_str */
        0,                                  /* tp_getattro */
        0,                                  /* tp_setattro */
        0,                                  /* tp_as_buffer */
        Py_TPFLAGS_DEFAULT |
        Py_TPFLAGS_BASETYPE |
        Py_TPFLAGS_HAVE_GC,                 /* tp_flags */
        "pkcp Class",                       /* tp_doc */
        (traverseproc) pkcp_traverse,       /* tp_traverse */
        (inquiry) pkcp_clear,               /* tp_clear */
        0,                                  /* tp_richcompare */
        0,                                  /* tp_weaklistoffset */
        0,                                  /* tp_iter */
        0,                                  /* tp_iternext */
        pkcp_methods,                       /* tp_methods */
        pkcp_members,                       /* tp_members */
        pkcp_getseters,                     /* tp_getset */
        0,                                  /* tp_base */
        0,                                  /* tp_dict */
        0,                                  /* tp_descr_get */
        0,                                  /* tp_descr_set */
        0,                                  /* tp_dictoffset */
        (initproc) pkcp_init,               /* tp_init */
        0,                                  /* tp_alloc */
        pkcp_new,                           /* tp_new */
};

static PyModuleDef pkcp_module = {
        PyModuleDef_HEAD_INIT,
        "pkcp._pkcp",
        "KCP - A Fast and Reliable ARQ Protocol",
        -1,
        NULL, NULL, NULL, NULL, NULL
};

PyMODINIT_FUNC PyInit__pkcp(void) {
    PyObject *m;

    if (PyType_Ready(&pkcpType) < 0)
        return NULL;

    m = PyModule_Create(&pkcp_module);
    if (m == NULL)
        return NULL;

    Py_INCREF(&pkcpType);
    PyModule_AddObject(m, "pkcp", (PyObject *) &pkcpType);
    return m;
}
