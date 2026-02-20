#include <Python.h>

#include "async.h"
#include "sleep_wrapper.h"
#include "sonyflake.h"


struct async_sonyflake_state {
	PyObject_HEAD

	struct sonyflake_state *sf;
	PyObject *sleep;
};

static int async_sonyflake_clear(PyObject *py_self) {
	struct async_sonyflake_state *self = (struct async_sonyflake_state *) py_self;
	Py_CLEAR(self->sf);
	Py_CLEAR(self->sleep);
	return 0;
}

static void async_sonyflake_dealloc(PyObject *py_self) {
	async_sonyflake_clear(py_self);

	PyTypeObject *tp = Py_TYPE(py_self);
	freefunc tp_free = PyType_GetSlot(tp, Py_tp_free);

	assert(tp_free != NULL);

	tp_free(py_self);
	Py_DECREF(tp);
}

static int async_sonyflake_traverse(PyObject *py_self, visitproc visit, void *arg) {
	struct async_sonyflake_state *self = (struct async_sonyflake_state *) py_self;
	Py_VISIT(self->sf);
	Py_VISIT(self->sleep);
	Py_VISIT(Py_TYPE(py_self));
	return 0;
}

static PyObject *async_sonyflake_new(PyTypeObject *type, PyObject *Py_UNUSED(args), PyObject *Py_UNUSED(kwargs)) {
	allocfunc tp_alloc = PyType_GetSlot(type, Py_tp_alloc);

	assert(tp_alloc != NULL);

	struct async_sonyflake_state *self = (void *) tp_alloc(type, 0);

	if (!self) {
		return NULL;
	}

	self->sf = NULL;
	self->sleep = NULL;

	return (PyObject *) self;
}

static int async_sonyflake_init(PyObject *py_self, PyObject *args, PyObject *Py_UNUSED(kwargs)) {
	struct async_sonyflake_state *self = (struct async_sonyflake_state *) py_self;

	if (!PyArg_ParseTuple(args, "OO", &self->sf, &self->sleep)) {
		return -1;
	}

	struct sonyflake_module_state *module = PyType_GetModuleState(Py_TYPE(self));

	assert(module != NULL);

	if (!PyObject_IsInstance((PyObject *) self->sf, module->sonyflake_cls)) {
		PyErr_SetString(PyExc_TypeError, "sf must be instance of SonyFlake");
		return -1;
	}

	if (!PyCallable_Check(self->sleep)) {
		PyErr_SetString(PyExc_TypeError, "sleep must be callable");
		return -1;
	}

	Py_INCREF(self->sf);
	Py_INCREF(self->sleep);

	return 0;
}

static inline PyObject *wrap_into_sleep(
	struct async_sonyflake_state *self,
	struct sonyflake_next_sleep_info *sleep_info,
	PyObject *obj
) {
	if (!obj) {
		return NULL;
	}

	struct sonyflake_module_state *module = PyType_GetModuleState(Py_TYPE(self));

	assert(module);
	assert(sleep_info);

	struct timespec diff = sleep_info->future;

	sub_diff(&diff, &sleep_info->now);

	PyObject *coro = PyObject_CallFunction(
		module->sleep_wrapper_cls,
		"OOd",
		obj,
		self->sleep,
		timespec_to_double(&diff)
	);

	Py_DECREF(obj);

	return coro;
}

static PyObject *async_sonyflake_call(struct async_sonyflake_state *self, PyObject *args) {
	struct sonyflake_next_sleep_info sleep_info;

	return wrap_into_sleep(self, &sleep_info, sonyflake_next_py(self->sf, args, &sleep_info));
}

static PyObject *async_sonyflake_await(struct async_sonyflake_state *self) {
	struct sonyflake_next_sleep_info sleep_info;

	return wrap_into_sleep(self, &sleep_info, sonyflake_next_py(self->sf, NULL, &sleep_info));
}

PyType_Slot async_sonyflake_type_slots[] = {
	{Py_tp_alloc, PyType_GenericAlloc},
	{Py_tp_dealloc, async_sonyflake_dealloc},
	{Py_tp_traverse, async_sonyflake_traverse},
	{Py_tp_clear, async_sonyflake_clear},
	{Py_tp_new, async_sonyflake_new},
	{Py_tp_init, async_sonyflake_init},
	{Py_tp_call, async_sonyflake_call},
	{Py_am_await, async_sonyflake_await},
	{Py_am_anext, async_sonyflake_await},
	{Py_am_aiter, PyObject_SelfIter},
	{0, 0},
};

PyType_Spec async_sonyflake_type_spec = {
	.name = MODULE_NAME ".AsyncSonyFlake",
	.basicsize = sizeof(struct async_sonyflake_state),
	.flags = Py_TPFLAGS_DEFAULT,
	.slots = async_sonyflake_type_slots,
};
