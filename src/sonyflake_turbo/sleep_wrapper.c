#include <Python.h>

#include "module.h"
#include "sleep_wrapper.h"

struct sleep_wrapper_state {
	PyObject_HEAD

	PyObject *obj;
	PyObject *sleep;
	PyObject *to_sleep;
	PyObject *it;
};

static int sleep_wrapper_clear(PyObject *py_self) {
	struct sleep_wrapper_state *self = (struct sleep_wrapper_state *) py_self;
	Py_CLEAR(self->obj);
	Py_CLEAR(self->sleep);
	Py_CLEAR(self->to_sleep);
	Py_CLEAR(self->it);
	return 0;
}

static void sleep_wrapper_dealloc(PyObject *py_self) {
	sleep_wrapper_clear(py_self);

	PyTypeObject *tp = Py_TYPE(py_self);
	freefunc tp_free = PyType_GetSlot(tp, Py_tp_free);

	assert(tp_free != NULL);

	tp_free(py_self);
	Py_DECREF(tp);
}

static int sleep_wrapper_traverse(PyObject *py_self, visitproc visit, void *arg) {
	struct sleep_wrapper_state *self = (struct sleep_wrapper_state *) py_self;
	Py_VISIT(self->obj);
	Py_VISIT(self->sleep);
	Py_VISIT(self->to_sleep);
	Py_VISIT(self->it);
	Py_VISIT(Py_TYPE(py_self));
	return 0;
}

static PyObject *sleep_wrapper_new(PyTypeObject *type, PyObject *Py_UNUSED(args), PyObject *Py_UNUSED(kwargs)) {
	allocfunc tp_alloc = PyType_GetSlot(type, Py_tp_alloc);

	assert(tp_alloc != NULL);

	struct sleep_wrapper_state *self = (void *) tp_alloc(type, 0);

	if (!self) {
		return NULL;
	}

	self->obj = NULL;
	self->sleep = NULL;
	self->to_sleep = NULL;
	self->it = NULL;

	return (PyObject *) self;
}

static int sleep_wrapper_init(PyObject *py_self, PyObject *args, PyObject *Py_UNUSED(kwargs)) {
	struct sleep_wrapper_state *self = (struct sleep_wrapper_state *) py_self;

	if (!PyArg_ParseTuple(args, "OOO", &self->obj, &self->sleep, &self->to_sleep)) {
		return -1;
	}

	Py_INCREF(self->obj);
	Py_INCREF(self->sleep);
	Py_INCREF(self->to_sleep);

	return 0;
}

static PyObject *sleep_wrapper_iternext(struct sleep_wrapper_state *self) {
	if (self->it) {
		PyObject *obj = PyIter_Next(self->it);

		if (PyErr_Occurred()) {
			return NULL;
		} else if (obj) {
			return obj;
		}

		Py_CLEAR(self->it);
	}

	PyErr_SetObject(PyExc_StopIteration, self->obj);

	return NULL;
}

static PySendResult sleep_wrapper_send(PyObject *py_self, PyObject *arg, PyObject **presult) {
	struct sleep_wrapper_state *self = (struct sleep_wrapper_state *) py_self;

	if (self->it) {
		PySendResult result = PyIter_Send(self->it, arg, presult);

		if (result != PYGEN_RETURN) {
			return result;
		}

		Py_CLEAR(self->it);
	}

	PyErr_SetObject(PyExc_StopIteration, self->obj);

	return PYGEN_ERROR;
}

static PyObject *sleep_wrapper_send_meth(PyObject *self, PyObject *arg)
{
	PyObject *result = NULL;

	if (sleep_wrapper_send(self, arg, &result) == PYGEN_RETURN) {
		return result;
	}

	return NULL;
}

static PyObject *sleep_wrapper_iter_or_await(PyObject *py_self) {
	struct sleep_wrapper_state *self = (struct sleep_wrapper_state *) py_self;

	Py_CLEAR(self->it);

	PyObject *coro = PyObject_CallFunction(self->sleep, "O", self->to_sleep);

	if (!coro) {
		return NULL;
	}

	unaryfunc am_await = PyType_GetSlot(Py_TYPE(coro), Py_am_await);

	if (!am_await) {
		PyErr_SetString(PyExc_TypeError, "sleep() does not support __await__");
		Py_DECREF(coro);
		return NULL;
	}

	self->it = am_await(coro);

	Py_DECREF(coro);

	if (!self->it) {
		return NULL;
	}

	if (!PyIter_Check(self->it)) {
		PyErr_SetString(PyExc_TypeError, "sleep().__await__() did not return an iterator");
		Py_CLEAR(self->it);
		return NULL;
	}

	Py_INCREF(py_self);

	return py_self;
}

static PyMethodDef sleep_wrapper_methods[] = {
	{"send", sleep_wrapper_send_meth, METH_O, ""},
	{NULL, NULL, 0, NULL}
};

PyType_Slot sleep_wrapper_type_slots[] = {
	{Py_tp_alloc, PyType_GenericAlloc},
	{Py_tp_dealloc, sleep_wrapper_dealloc},
	{Py_tp_traverse, sleep_wrapper_traverse},
	{Py_tp_clear, sleep_wrapper_clear},
	{Py_tp_iter, sleep_wrapper_iter_or_await},
	{Py_tp_iternext, sleep_wrapper_iternext},
	{Py_tp_new, sleep_wrapper_new},
	{Py_tp_init, sleep_wrapper_init},
	{Py_am_await, sleep_wrapper_iter_or_await},
	{Py_am_send, sleep_wrapper_send},
	{Py_tp_methods, sleep_wrapper_methods},
	{0, 0},
};

PyType_Spec sleep_wrapper_type_spec = {
	.name = MODULE_NAME ".sleep_wrapper",
	.basicsize = sizeof(struct sleep_wrapper_state),
	.flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
	.slots = sleep_wrapper_type_slots,
};
