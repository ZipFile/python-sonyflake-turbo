#pragma once

#include <Python.h>

#include "sonyflake.h"

struct sonyflake_aiter_state;
extern PyType_Slot sonyflake_aiter_type_slots[];
extern PyType_Spec sonyflake_aiter_type_spec;
