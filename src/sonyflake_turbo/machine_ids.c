#include <Python.h>
#include <pythread.h>
#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#include "machine_ids.h"

struct machine_id_lcg_state {
	PyObject_HEAD
	atomic_uint machine_id;
};

int cmp_machine_ids(const void *a, const void *b) {
	return (*(uint16_t *)a - *(uint16_t *)b);
}

void sort_machine_ids(uint16_t *machine_ids, int machine_ids_len) {
	if (machine_ids_len <= 1) {
		return;
	}

	qsort(machine_ids, machine_ids_len, sizeof(uint16_t), cmp_machine_ids);
}

bool has_machine_id_dupes(const uint16_t *machine_ids, int machine_ids_len) {
	if (machine_ids_len <= 1) {
		return false;
	}

	for (int i = 1; i < machine_ids_len; i++) {
		if (machine_ids[i] == machine_ids[i - 1]) {
			return true;
		}
	}

	return false;
}

static PyObject *machine_id_lcg_new(PyTypeObject *type, PyObject *args, PyObject *Py_UNUSED(kwargs)) {
	unsigned int seed = 0;

	if (!PyArg_ParseTuple(args, "I", &seed)) {
		return NULL;
	}

	allocfunc tp_alloc = PyType_GetSlot(type, Py_tp_alloc);

	assert(tp_alloc != NULL);

	struct machine_id_lcg_state *self = (void *) tp_alloc(type, 0);

	if (!self) {
		return NULL;
	}

	atomic_init(&self->machine_id, seed);

	return (PyObject *) self;
}

static void machine_id_lcg_dealloc(PyObject *self) {
	PyTypeObject *tp = Py_TYPE(self);
	freefunc tp_free = PyType_GetSlot(tp, Py_tp_free);

	assert(tp_free != NULL);

	tp_free(self);
	Py_DECREF(tp);
}

static PyObject *machine_id_lcg_next(struct machine_id_lcg_state *self) {
	return PyLong_FromLong(machine_id_lcg_atomic(&self->machine_id));
}

static PyObject *machine_id_lcg_repr(struct machine_id_lcg_state *self) {
	return PyUnicode_FromFormat(
		"MachineIDLCG(%u)",
		atomic_load_explicit(&self->machine_id, memory_order_relaxed)
	);
}

static PyObject *machine_id_lcg_call(struct machine_id_lcg_state *self, PyObject *Py_UNUSED(args), PyObject *Py_UNUSED(kwargs)) {
	return machine_id_lcg_next(self);
}

PyDoc_STRVAR(machine_id_lcg_doc,
"MachineIDLCG(seed, /)\n--\n\n"
"LCG with params a=32309, c=13799, m=65536.\n"
"Provides a thread-safe way to generate pseudo-random sequence of machine ids to be used as args to SonyFlake.\n"
"\n"
"You generally want to use this class as a singleton, i.e. create one instance and reuse it.\n"
);

PyType_Slot machine_id_lcg_slots[] = {
	{Py_tp_alloc, PyType_GenericAlloc},
	{Py_tp_dealloc, machine_id_lcg_dealloc},
	{Py_tp_iter, PyObject_SelfIter},
	{Py_tp_iternext, machine_id_lcg_next},
	{Py_tp_new, machine_id_lcg_new},
	{Py_tp_call, machine_id_lcg_call},
	{Py_tp_doc, (void *) machine_id_lcg_doc},
	{Py_tp_repr, machine_id_lcg_repr},
	{0, 0},
};

PyType_Spec machine_id_lcg_spec = {
	.name = MODULE_NAME ".MachineIDLCG",
	.basicsize = sizeof(struct machine_id_lcg_state),
	.flags = Py_TPFLAGS_DEFAULT,
	.slots = machine_id_lcg_slots,
};
