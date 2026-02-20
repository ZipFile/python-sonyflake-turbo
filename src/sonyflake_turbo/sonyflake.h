#pragma once

#include <Python.h>
#include <stdint.h>
#include <time.h>

#define MODULE_NAME "sonyflake_turbo"
#define SONYFLAKE_EPOCH 1409529600 // 2014-09-01 00:00:00 UTC
#define SONYFLAKE_SEQUENCE_BITS 8
#define SONYFLAKE_SEQUENCE_MAX ((1 << SONYFLAKE_SEQUENCE_BITS) - 1)
#define SONYFLAKE_MACHINE_ID_BITS 16
#define SONYFLAKE_MACHINE_ID_MAX ((1 << SONYFLAKE_MACHINE_ID_BITS) - 1)
#define SONYFLAKE_MACHINE_ID_OFFSET SONYFLAKE_SEQUENCE_BITS
#define SONYFLAKE_TIME_OFFSET (SONYFLAKE_MACHINE_ID_BITS + SONYFLAKE_SEQUENCE_BITS)

typedef uint64_t sonyflake_time;
struct sonyflake_state;
extern PyType_Slot sonyflake_type_slots[];
extern PyType_Spec sonyflake_type_spec;
extern PyModuleDef_Slot sonyflake_slots[];
extern struct PyModuleDef sonyflake_module;

static const struct timespec default_start_time = {
	.tv_sec = SONYFLAKE_EPOCH,
	.tv_nsec = 0
};

struct sonyflake_next_sleep_info {
	struct timespec now;
	struct timespec future;
};

#define print_time(name, ts) printf("%s: %ld.%09ld\n", name, (long) ts.tv_sec, (long) ts.tv_nsec)

static inline sonyflake_time to_sonyflake_time(const struct timespec *ts) {
	return ts->tv_sec * 100 + ts->tv_nsec / 10000000;
}

static inline void from_sonyflake_time(struct timespec *start_time,  sonyflake_time sf_time, struct timespec *ts) {
	ts->tv_sec = start_time->tv_sec + sf_time / 100;
	ts->tv_nsec = start_time->tv_nsec + (sf_time % 100) * 10000000;

	if (ts->tv_nsec >= 1000000000) {
		ts->tv_sec++;
		ts->tv_nsec -= 1000000000;
	}
}

static inline void sub_diff(struct timespec *a, const struct timespec *b) {
	if (a->tv_sec < b->tv_sec) {
		a->tv_sec = 0;
		a->tv_nsec = 0;
		return;
	}

	a->tv_sec -= b->tv_sec;

	if (a->tv_sec == 0 && a->tv_nsec < b->tv_nsec) {
		a->tv_nsec = 0;
		return;
	}

	a->tv_nsec -= b->tv_nsec;

	if (a->tv_nsec < 0) {
		a->tv_sec--;
		a->tv_nsec += 1000000000;
	}
}

PyObject *sonyflake_next(struct sonyflake_state *self, struct sonyflake_next_sleep_info *sleep_info);
PyObject *sonyflake_next_n(struct sonyflake_state *self, Py_ssize_t n, struct sonyflake_next_sleep_info *sleep_info);
