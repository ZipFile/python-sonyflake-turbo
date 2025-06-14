#include <Python.h>
#include <pythread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#define SONYFLAKE_EPOCH 1409529600 // 2014-09-01 00:00:00 UTC
#define SONYFLAKE_SEQUENCE_BITS 8
#define SONYFLAKE_SEQUENCE_MAX ((1 << SONYFLAKE_SEQUENCE_BITS) - 1)
#define SONYFLAKE_MACHINE_ID_BITS 16
#define SONYFLAKE_MACHINE_ID_MAX ((1 << SONYFLAKE_MACHINE_ID_BITS) - 1)
#define SONYFLAKE_MACHINE_ID_OFFSET SONYFLAKE_SEQUENCE_BITS
#define SONYFLAKE_TIME_OFFSET (SONYFLAKE_MACHINE_ID_BITS + SONYFLAKE_SEQUENCE_BITS)

typedef uint64_t sonyflake_time;

struct sonyflake_state {
	PyObject_HEAD
	PyThread_type_lock *lock;

	struct timespec start_time;
	sonyflake_time elapsed_time;
	uint32_t combined_sequence;
	uint16_t *machine_ids;
	size_t machine_ids_len;
};

const struct timespec default_start_time = {
	.tv_sec = SONYFLAKE_EPOCH,
	.tv_nsec = 0
};

inline sonyflake_time to_sonyflake_time(const struct timespec *ts) {
	return ts->tv_sec * 100 + ts->tv_nsec / 10000000;
}

inline void from_sonyflake_time(sonyflake_time sf_time, struct timespec *ts) {
	ts->tv_sec = sf_time / 100;
	ts->tv_nsec = (sf_time % 100) * 10000000;
}

inline void sub_diff(struct timespec *a, const struct timespec *b) {
	a->tv_sec -= b->tv_sec;
	a->tv_nsec -= b->tv_nsec;

	if (a->tv_nsec < 0) {
		a->tv_sec--;
		a->tv_nsec += 1000000000;
	}
}

inline useconds_t get_time_to_usleep(const struct timespec *diff) {
	return diff->tv_sec * 1000000 + diff->tv_nsec / 1000;
}

int cmp_machine_ids(const void *a, const void *b) {
	return (*(uint16_t *)a - *(uint16_t *)b);
}

uint64_t compose(const struct sonyflake_state *self) {
	uint64_t machine_id = self->machine_ids[self->combined_sequence >> SONYFLAKE_SEQUENCE_BITS];
	uint64_t sequence = self->combined_sequence & SONYFLAKE_SEQUENCE_MAX;

	return (self->elapsed_time << SONYFLAKE_TIME_OFFSET) | (machine_id << SONYFLAKE_MACHINE_ID_OFFSET) | sequence;
}

bool incr_combined_sequence(struct sonyflake_state *self) {
	self->combined_sequence = (self->combined_sequence + 1) % (self->machine_ids_len * (1 << SONYFLAKE_SEQUENCE_BITS));

	return self->combined_sequence == 0;
}

void sort_machine_ids(uint16_t *machine_ids, size_t machine_ids_len) {
	if (machine_ids_len <= 1) {
		return;
	}

	qsort(machine_ids, machine_ids_len, sizeof(uint16_t), cmp_machine_ids);
}

bool has_machine_id_dupes(const uint16_t *machine_ids, size_t machine_ids_len) {
	if (machine_ids_len <= 1) {
		return false;
	}

	for (size_t i = 1; i < machine_ids_len; i++) {
		if (machine_ids[i] == machine_ids[i - 1]) {
			return true;
		}
	}

	return false;
}

static PyObject *sonyflake_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) {
	allocfunc tp_alloc = PyType_GetSlot(type, Py_tp_alloc);

	assert(tp_alloc != NULL);

	struct sonyflake_state *self = (void *) tp_alloc(type, 0);

	if (!self) {
		return NULL;
	}

	self->lock = PyThread_allocate_lock();

	if (!self->lock) {
		Py_DECREF(self);
		PyErr_SetString(PyExc_MemoryError, "Unable to allocate lock");

		return NULL;
	}

	self->start_time = default_start_time;
	self->elapsed_time = 0;
	self->combined_sequence = 0;
	self->machine_ids = NULL;
	self->machine_ids_len = 0;

	return (PyObject *) self;
}

static int sonyflake_init(PyObject *py_self, PyObject *args, PyObject *kwargs) {
	struct sonyflake_state *self = (struct sonyflake_state *) py_self;
	PyObject *machine_id_item = NULL;
	PyObject *start_time_obj = NULL;
	Py_ssize_t machine_ids_len = 0;
	long long start_time = 0;
	long machine_id = 0;
	Py_ssize_t i = 0;

	machine_ids_len = PyTuple_Size(args);

	if (machine_ids_len < 1) {
		PyErr_SetString(PyExc_ValueError, "At least one machine ID must be provided");
		return -1;
	}

	if (machine_ids_len > 65536) {
		PyErr_SetString(PyExc_ValueError, "Too many machine IDs, maximum is 65536");
		return -1;
	}

	self->machine_ids = PyMem_Malloc(machine_ids_len * sizeof(uint16_t));

	if (self->machine_ids == NULL) {
		PyErr_SetString(PyExc_MemoryError, "Failed to allocate memory for machine IDs");
		return -1;
	}

	for (i = 0; i < machine_ids_len; i++) {
		machine_id_item = PyTuple_GetItem(args, i);

		if (!PyLong_Check(machine_id_item)) {
			PyErr_SetString(PyExc_TypeError, "Machine IDs must be integers");
			goto err;
		}

		machine_id = PyLong_AsLong(machine_id_item);

		if (machine_id < 0 || machine_id > SONYFLAKE_MACHINE_ID_MAX) {
			PyErr_SetString(PyExc_ValueError, "Machine IDs must be in range [0, 65535]");
			goto err;
		}

		self->machine_ids[i] = (uint16_t) machine_id;
	}

	self->machine_ids_len = (size_t) machine_ids_len;

	sort_machine_ids(self->machine_ids, self->machine_ids_len);

	if (has_machine_id_dupes(self->machine_ids, self->machine_ids_len)) {
		PyErr_SetString(PyExc_ValueError, "Duplicate machine IDs are not allowed");
		goto err;
	}

	if (kwargs == NULL) {
		return 0;
	}

	start_time_obj = PyDict_GetItemString(kwargs, "start_time");

	if (start_time_obj == NULL || start_time_obj == Py_None) {
		return 0;
	}

	if (!PyLong_Check(start_time_obj)) {
		PyErr_SetString(PyExc_TypeError, "start_time must be an integer");
		goto err;
	}

	start_time = PyLong_AsLongLong(start_time_obj);

	if (PyErr_Occurred()) {
		goto err;
	}

	self->start_time.tv_sec = start_time;
	self->start_time.tv_nsec = 0;

	return 0;

err:
	if (self->machine_ids) {
		PyMem_Free(self->machine_ids);
		self->machine_ids = NULL;
		self->machine_ids_len = 0;
	}

	return -1;
}

static void sonyflake_dealloc(struct sonyflake_state *self) {
	if (self->lock) {
		PyThread_free_lock(self->lock);
	}

	PyTypeObject *tp = Py_TYPE(self);
	freefunc tp_free = PyType_GetSlot(tp, Py_tp_free);

	assert(tp_free != NULL);

	if (self->machine_ids) {
		PyMem_Free(self->machine_ids);
	}

	tp_free(self);
	Py_DECREF(tp);
}

static PyObject *sonyflake_next(struct sonyflake_state *self) {
	struct timespec now, future;
	useconds_t to_sleep = 0;
	sonyflake_time current;
	uint64_t sonyflake_id;

	clock_gettime(CLOCK_REALTIME, &now);

	PyThread_acquire_lock(self->lock, 1);

	sub_diff(&now, &self->start_time);

	current = to_sonyflake_time(&now);

	if (self->elapsed_time < current) {
		self->elapsed_time = current;
		self->combined_sequence = 0;
	} else if (incr_combined_sequence(self)) {
		self->elapsed_time++;

		from_sonyflake_time(self->elapsed_time, &future);
		sub_diff(&future, &now);

		to_sleep = get_time_to_usleep(&future);
	}

	sonyflake_id = compose(self);

	PyThread_release_lock(self->lock);

	if (to_sleep > 0) {
		Py_BEGIN_ALLOW_THREADS;
		usleep(to_sleep);
		Py_END_ALLOW_THREADS;
	}

	return PyLong_FromUnsignedLongLong(sonyflake_id);
}

static PyObject *sonyflake_repr(struct sonyflake_state *self) {
	PyObject *s, *args_list = PyList_New(self->machine_ids_len + 1);

	if (!args_list) {
		return NULL;
	}

	s = PyUnicode_FromFormat("start_time=%ld", (long) self->start_time.tv_sec);

	if (!s) {
		Py_DECREF(args_list);
		return NULL;
	}

	PyList_SetItem(args_list, self->machine_ids_len, s);

	for (Py_ssize_t i = 0; i < self->machine_ids_len; i++) {
		s = PyUnicode_FromFormat("%u", (unsigned) self->machine_ids[i]);

		if (!s) {
			Py_DECREF(args_list);
			return NULL;
		}

		PyList_SetItem(args_list, i, s);
	}

	s = PyUnicode_FromString(", ");

	if (!s) {
		Py_DECREF(args_list);
		return NULL;
	}

	PyObject *args_str = PyUnicode_Join(s, args_list);

	Py_DECREF(args_list);
	Py_DECREF(s);

	if (!args_str) {
		return NULL;
	}

	s = PyUnicode_FromFormat("SonyFlake(%U)", args_str);

	Py_DECREF(args_str);

	return s;
}

PyDoc_STRVAR(sonyflake_doc,
"SonyFlake(*machine_id, start_time=None)\n--\n\n"
"SonyFlake ID generator implementation that combines multiple ID generators into one to improve throughput.\n"
"Upon counter overflow, it switches to the next machine ID and sleeps only when all machine ids are exhausted for time"
"frame of 10ms. This produces always-increasing id sequence.\n"
);

static PyType_Slot sonyflake_type_slots[] = {
	{Py_tp_alloc, PyType_GenericAlloc},
	{Py_tp_dealloc, sonyflake_dealloc},
	{Py_tp_iter, PyObject_SelfIter},
	{Py_tp_iternext, sonyflake_next},
	{Py_tp_new, sonyflake_new},
	{Py_tp_init, sonyflake_init},
	{Py_tp_doc, sonyflake_doc},
	{Py_tp_repr, sonyflake_repr},
	{0, 0},
};

static PyType_Spec sonyflake_type_spec = {
	.name = "sonyflake_turbo.SonyFlake",
	.basicsize = sizeof(struct sonyflake_state),
	.flags = Py_TPFLAGS_DEFAULT,
	.slots = sonyflake_type_slots,
};

static struct PyModuleDef sonyflake_module = {
	PyModuleDef_HEAD_INIT,
	.m_name = "sonyflake_turbo",
	.m_doc = "",
	.m_size = -1,
};

inline uint16_t machine_id_lcg(uint32_t x) {
	return (32309 * x + 13799) % 65536;
}

inline uint16_t machine_id_lcg_atomic(atomic_uint *x) {
	uint32_t old, new;
	do {
		old = atomic_load_explicit(x, memory_order_relaxed);
		new = machine_id_lcg(old);
	} while (!atomic_compare_exchange_weak_explicit(x, &old, new, memory_order_relaxed, memory_order_relaxed));
	return new;
}

struct machine_id_lcg_state {
	PyObject_HEAD
	atomic_uint machine_id;
};

static PyObject *machine_id_lcg_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) {
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

	atomic_init(&self->machine_id, machine_id_lcg((uint32_t) seed));

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

static PyObject *machine_id_lcg_call(struct machine_id_lcg_state *self, PyObject *args, PyObject *kwargs) {
	return machine_id_lcg_next(self);
}

PyDoc_STRVAR(machine_id_lcg_doc,
"MachineIDLCG(seed, /)\n--\n\n"
"LCG with params a=32309, c=13799, m=65536.\n"
"Provides a thread-safe way to generate pseudo-random sequence of machine ids to be used as args to SonyFlake.\n"
);


static PyType_Slot machine_id_lcg_slots[] = {
	{Py_tp_alloc, PyType_GenericAlloc},
	{Py_tp_dealloc, machine_id_lcg_dealloc},
	{Py_tp_iter, PyObject_SelfIter},
	{Py_tp_iternext, machine_id_lcg_next},
	{Py_tp_new, machine_id_lcg_new},
	{Py_tp_call, machine_id_lcg_call},
	{Py_tp_doc, machine_id_lcg_doc},
	{Py_tp_repr, machine_id_lcg_repr},
	{0, 0},
};

static PyType_Spec machine_id_lcg_spec = {
	.name = "sonyflake_turbo.MachineIDLCG",
	.basicsize = sizeof(struct machine_id_lcg_state),
	.flags = Py_TPFLAGS_DEFAULT,
	.slots = machine_id_lcg_slots,
};

PyMODINIT_FUNC
PyInit_sonyflake_turbo(void)
{
	PyObject *sonyflake_cls, *machine_id_lcg_cls;
	PyObject *module = PyModule_Create(&sonyflake_module);

	if (!module) {
		return NULL;
	}

#if defined(Py_GIL_DISABLED) && !defined(Py_LIMITED_API)
	PyUnstable_Module_SetGIL(module, Py_MOD_GIL_NOT_USED);
#endif

	sonyflake_cls = PyType_FromSpec(&sonyflake_type_spec);

	if (!sonyflake_cls) {
		goto err;
	}

	if (PyModule_AddObject(module, "SonyFlake", sonyflake_cls) < 0) {
		goto err_sf;
	}

	machine_id_lcg_cls = PyType_FromSpec(&machine_id_lcg_spec);

	if (!machine_id_lcg_cls) {
		goto err_lcg;
	}

	if (PyModule_AddObject(module, "MachineIDLCG", machine_id_lcg_cls) < 0) {
		goto err_lcg;
	}

	PyModule_AddIntMacro(module, SONYFLAKE_EPOCH);
	PyModule_AddIntMacro(module, SONYFLAKE_SEQUENCE_BITS);
	PyModule_AddIntMacro(module, SONYFLAKE_SEQUENCE_MAX);
	PyModule_AddIntMacro(module, SONYFLAKE_MACHINE_ID_BITS);
	PyModule_AddIntMacro(module, SONYFLAKE_MACHINE_ID_MAX);
	PyModule_AddIntMacro(module, SONYFLAKE_MACHINE_ID_OFFSET);
	PyModule_AddIntMacro(module, SONYFLAKE_TIME_OFFSET);

	return module;

err_lcg:
	Py_DECREF(machine_id_lcg_cls);
err_sf:
	Py_DECREF(sonyflake_cls);
err:
	Py_DECREF(module);
	return NULL;
}
