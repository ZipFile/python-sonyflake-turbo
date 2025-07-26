#pragma once

#include <Python.h>
#include <stdatomic.h>
#include <stdint.h>

#include "sonyflake.h"

struct machine_id_lcg_state;
extern PyType_Slot machine_id_lcg_slots[];
extern PyType_Spec machine_id_lcg_spec;

void sort_machine_ids(uint16_t *machine_ids, size_t machine_ids_len);
bool has_machine_id_dupes(const uint16_t *machine_ids, size_t machine_ids_len);

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
