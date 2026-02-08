#include <Python.h>
#include <pythread.h>
#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#if !__STDC_NO_THREADS__
#include <threads.h>
#endif

#include "sonyflake.h"
#include "machine_ids.h"

struct sonyflake_state {
	PyObject_HEAD
	PyThread_type_lock *lock;

	struct timespec start_time;
	sonyflake_time elapsed_time;
	uint32_t combined_sequence;
	uint16_t *machine_ids;
	Py_ssize_t machine_ids_len;
};

uint64_t compose(const struct sonyflake_state *self) {
	uint64_t machine_id = self->machine_ids[self->combined_sequence >> SONYFLAKE_SEQUENCE_BITS];
	uint64_t sequence = self->combined_sequence & SONYFLAKE_SEQUENCE_MAX;

	return (self->elapsed_time << SONYFLAKE_TIME_OFFSET) | (machine_id << SONYFLAKE_MACHINE_ID_OFFSET) | sequence;
}

bool incr_combined_sequence(struct sonyflake_state *self) {
	self->combined_sequence = (self->combined_sequence + 1) % (self->machine_ids_len * (1 << SONYFLAKE_SEQUENCE_BITS));

	return self->combined_sequence == 0;
}

static inline void get_relative_current_time(struct sonyflake_state *self, struct timespec *now) {
	timespec_get(now, TIME_UTC);
	sub_diff(now, &self->start_time);
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

	self->machine_ids_len = machine_ids_len;

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

static PyObject *sonyflake_sleep(PyObject *obj, struct timespec *to_nanosleep) {
	if (!obj) {
		return NULL;
	}

	if (!to_nanosleep) {
		return obj;
	}

	if (to_nanosleep->tv_sec == 0 && to_nanosleep->tv_nsec == 0) {
		return obj;
	}

	int ret = 0;

	Py_BEGIN_ALLOW_THREADS;
#if __STDC_NO_THREADS__
	ret = nanosleep(to_nanosleep, NULL);
#else
	ret = thrd_sleep(to_nanosleep, NULL);
#endif
	Py_END_ALLOW_THREADS;

	if (ret == 0) {
		return obj;
	}

#if __STDC_NO_THREADS__
	if (ret < 0 && errno == EINTR && PyErr_CheckSignals()) {
		goto err;
	}
#else
	if (ret == -1) {
		if (PyErr_CheckSignals()) {
			goto err;
		}

		return obj;
	}
#endif

	PyErr_SetFromErrno(PyExc_OSError);
err:
	Py_DECREF(obj);
	return NULL;
}

PyObject *sonyflake_next(struct sonyflake_state *self, struct timespec *to_nanosleep) {
	struct timespec now, future;
	sonyflake_time current;
	uint64_t sonyflake_id;

	PyThread_acquire_lock(self->lock, 1);

	get_relative_current_time(self, &now);

	current = to_sonyflake_time(&now);

	if (to_nanosleep) {
		to_nanosleep->tv_sec = 0;
		to_nanosleep->tv_nsec = 0;
	}

	if (self->elapsed_time < current) {
		self->elapsed_time = current;
		self->combined_sequence = 0;
	} else if (incr_combined_sequence(self)) {
		self->elapsed_time++;

		if (to_nanosleep) {
			from_sonyflake_time(self->elapsed_time, &future);
			sub_diff(&future, &now);

			*to_nanosleep = future;
		}
	}

	sonyflake_id = compose(self);

	PyThread_release_lock(self->lock);

	return PyLong_FromUnsignedLongLong(sonyflake_id);
}

PyObject *sonyflake_next_n(struct sonyflake_state *self, Py_ssize_t n, struct timespec *to_nanosleep) {
	assert(n > 0);

	PyObject *out = PyList_New(n);

	if (!out) {
		return NULL;
	}

	struct timespec now, future;
	sonyflake_time current, diff;

	PyThread_acquire_lock(self->lock, 1);

	get_relative_current_time(self, &now);

	current = to_sonyflake_time(&now);

	if (self->elapsed_time < current) {
		self->elapsed_time = current;
		self->combined_sequence = 0;
	} else if (incr_combined_sequence(self)) {
		self->elapsed_time++;
	}

	PyList_SetItem(out, 0, PyLong_FromUnsignedLongLong(compose(self)));

	for (Py_ssize_t i = 1; i < n; i++) {
		if (incr_combined_sequence(self)) {
			self->elapsed_time++;
		}

		PyList_SetItem(out, i, PyLong_FromUnsignedLongLong(compose(self)));
	}

	if (!to_nanosleep) {
		goto end;
	}

	to_nanosleep->tv_sec = 0;
	to_nanosleep->tv_nsec = 0;
	diff = self->elapsed_time - current;

	if (diff <= 0) {
		goto end;
	} else if (diff > 1) {
		get_relative_current_time(self, &now);
	}

	from_sonyflake_time(self->elapsed_time, &future);
	sub_diff(&future, &now);

	*to_nanosleep = future;

end:
	PyThread_release_lock(self->lock);

	return out;
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

static PyObject *sonyflake_iternext(struct sonyflake_state *self) {
	struct timespec to_nanosleep = { 0, 0 };
	PyObject *sonyflake_id = sonyflake_next(self, &to_nanosleep);

	return sonyflake_sleep(sonyflake_id, &to_nanosleep);
}

static PyObject *sonyflake_call(struct sonyflake_state *self, PyObject *args) {
	Py_ssize_t n = 0;

	if (!PyArg_ParseTuple(args, "n", &n)) {
		return NULL;
	}

	if (n <= 0) {
		PyErr_SetString(PyExc_ValueError, "n must be positive");
		return NULL;
	}

	struct timespec to_nanosleep = { 0, 0 };
	PyObject *sonyflake_ids = sonyflake_next_n(self, n, &to_nanosleep);

	return sonyflake_sleep(sonyflake_ids, &to_nanosleep);
}

PyDoc_STRVAR(sonyflake_doc,
"SonyFlake(*machine_id, start_time=None)\n--\n\n"
"SonyFlake ID generator implementation that combines multiple ID generators into one to improve throughput.\n"
"Upon counter overflow, it switches to the next machine ID and sleeps only when all machine ids are exhausted for time"
"frame of 10ms. This produces always-increasing id sequence.\n"
"\n"
"Important:\n"
"    Implementation uses thread locks and blocking sleeps.\n"
);

PyType_Slot sonyflake_type_slots[] = {
	{Py_tp_alloc, PyType_GenericAlloc},
	{Py_tp_dealloc, sonyflake_dealloc},
	{Py_tp_iter, PyObject_SelfIter},
	{Py_tp_iternext, sonyflake_iternext},
	{Py_tp_new, sonyflake_new},
	{Py_tp_init, sonyflake_init},
	{Py_tp_call, sonyflake_call},
	{Py_tp_doc, sonyflake_doc},
	{Py_tp_repr, sonyflake_repr},
	{0, 0},
};

PyType_Spec sonyflake_type_spec = {
	.name = MODULE_NAME ".SonyFlake",
	.basicsize = sizeof(struct sonyflake_state),
	.flags = Py_TPFLAGS_DEFAULT,
	.slots = sonyflake_type_slots,
};
