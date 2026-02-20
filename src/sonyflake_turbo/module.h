#pragma once

#include <Python.h>
#include <assert.h>

#define MODULE_NAME "sonyflake_turbo"
#define MODULE(self) ((struct sonyflake_module_state *) PyType_GetModuleState(Py_TYPE(self)))

struct sonyflake_module_state {
	PyObject *sonyflake_cls;
	PyObject *machine_id_lcg_cls;
	PyObject *sleep_wrapper_cls;
};

extern struct PyModuleDef sonyflake_module;
