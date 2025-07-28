#include <Python.h>

#include "sonyflake.h"
#include "async_iter.h"

struct sonyflake_aiter_state {
	PyObject_HEAD

	struct sonyflake_state *sonyflake;
	PyObject *sleep_await_iter;
	PyObject *sonyflake_id;
};

static int sonyflake_aiter_clear(PyObject *py_self) {
	struct sonyflake_aiter_state *self = (struct sonyflake_aiter_state *) py_self;
	Py_CLEAR(self->sonyflake);
	Py_CLEAR(self->sleep_await_iter);
	Py_CLEAR(self->sonyflake_id);
	return 0;
}

static void sonyflake_aiter_dealloc(PyObject *py_self) {
	sonyflake_aiter_clear(py_self);

	PyTypeObject *tp = Py_TYPE(py_self);
	freefunc tp_free = PyType_GetSlot(tp, Py_tp_free);

	assert(tp_free != NULL);

	tp_free(py_self);
	Py_DECREF(tp);
}

static int sonyflake_aiter_traverse(PyObject *py_self, visitproc visit, void *arg) {
	struct sonyflake_aiter_state *self = (struct sonyflake_aiter_state *) py_self;
	Py_VISIT(self->sonyflake);
	Py_VISIT(self->sleep_await_iter);
	Py_VISIT(self->sonyflake_id);
	Py_VISIT(Py_TYPE(py_self));
	return 0;
}

static PyObject *sonyflake_aiter_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) {
	allocfunc tp_alloc = PyType_GetSlot(type, Py_tp_alloc);

	assert(tp_alloc != NULL);

	struct sonyflake_aiter_state *self = (void *) tp_alloc(type, 0);

	if (!self) {
		return NULL;
	}

	self->sonyflake = NULL;
	self->sonyflake_id = NULL;
	self->sleep_await_iter = NULL;

	return (PyObject *) self;
}

static int sonyflake_aiter_init(PyObject *py_self, PyObject *args, PyObject *Py_UNUSED(kwargs)) {
	struct sonyflake_aiter_state *self = (struct sonyflake_aiter_state *) py_self;

	if (!PyArg_ParseTuple(args, "O", &self->sonyflake)) {
		return -1;
	}

	return 0;
}

static PyObject *sonyflake_aiter_iternext(struct sonyflake_aiter_state *self) {
	assert(self->sonyflake);

	if (self->sleep_await_iter) {
		PyObject *obj = PyIter_Next(self->sleep_await_iter);

		if (PyErr_Occurred()) {
			return NULL;
		} else if (obj) {
			return obj;
		}

		Py_CLEAR(self->sleep_await_iter);
	}

	PyErr_SetObject(PyExc_StopIteration, self->sonyflake_id);

	return NULL;
}

static PySendResult sonyflake_aiter_send(PyObject *py_self, PyObject *arg, PyObject **presult) {
	struct sonyflake_aiter_state *self = (struct sonyflake_aiter_state *) py_self;

	assert(self->sonyflake);

	if (self->sleep_await_iter) {
		PySendResult result = PyIter_Send(self->sleep_await_iter, arg, presult);

		if (result != PYGEN_RETURN) {
			return result;
		}

		Py_CLEAR(self->sleep_await_iter);
	}

	PyErr_SetObject(PyExc_StopIteration, self->sonyflake_id);

	return PYGEN_ERROR;
}

static PyObject *sonyflake_aiter_send_meth(PyObject *self, PyObject *arg)
{
	PyObject *result = NULL;

	if (sonyflake_aiter_send(self, arg, &result) == PYGEN_RETURN) {
		return result;
	}

	return NULL;
}

static PyObject *sonyflake_aiter_anext(PyObject *py_self) {
	struct sonyflake_aiter_state *self = (struct sonyflake_aiter_state *) py_self;
	uint64_t to_usleep = 0;

	assert(self->sonyflake);

	Py_CLEAR(self->sonyflake_id);

	self->sonyflake_id = sonyflake_next(self->sonyflake, &to_usleep);

	if (!self->sonyflake_id) {
		return NULL;
	}

	if (to_usleep > 0) {
		PyObject *async_sleep = sonyflake_get_async_sleep(self->sonyflake);

		assert(async_sleep);
		Py_CLEAR(self->sleep_await_iter);

		PyObject *coro = PyObject_CallFunction(async_sleep, "d", ((double) to_usleep) / 1000000.0);

		if (!coro) {
			return NULL;
		}

		unaryfunc am_await = PyType_GetSlot(Py_TYPE(coro), Py_am_await);

		if (!am_await) {
			PyErr_SetString(PyExc_TypeError, "async_sleep() does not support __await__");
			Py_DECREF(coro);
			return NULL;
		}

		self->sleep_await_iter = am_await(coro);

		Py_DECREF(coro);

		if (!self->sleep_await_iter) {
			return NULL;
		}

		if (!PyIter_Check(self->sleep_await_iter)) {
			PyErr_SetString(PyExc_TypeError, "sleep().__await__() did not return an iterator");
			Py_CLEAR(self->sleep_await_iter);
			return NULL;
		}
	}

	Py_INCREF(py_self);

	return py_self;
}

static PyMethodDef sonyflake_aiter_methods[] = {
	{"send", sonyflake_aiter_send_meth, METH_O, ""},
	{NULL, NULL}
};

PyType_Slot sonyflake_aiter_type_slots[] = {
	{Py_tp_alloc, PyType_GenericAlloc},
	{Py_tp_dealloc, sonyflake_aiter_dealloc},
	{Py_tp_traverse, sonyflake_aiter_traverse},
	{Py_tp_clear, sonyflake_aiter_clear},
	{Py_tp_iter, PyObject_SelfIter},
	{Py_tp_iternext, sonyflake_aiter_iternext},
	{Py_tp_new, sonyflake_aiter_new},
	{Py_tp_init, sonyflake_aiter_init},
	{Py_am_await, PyObject_SelfIter},
	{Py_am_anext, sonyflake_aiter_anext},
	{Py_am_send, sonyflake_aiter_send},
	{Py_tp_methods, sonyflake_aiter_methods},
	{0, 0},
};

PyType_Spec sonyflake_aiter_type_spec = {
	.name = MODULE_NAME ".SonyFlakeAiter",
	.basicsize = sizeof(struct sonyflake_aiter_state),
	.flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
	.slots = sonyflake_aiter_type_slots,
};
