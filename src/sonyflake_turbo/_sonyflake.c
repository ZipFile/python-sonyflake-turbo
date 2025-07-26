#include <Python.h>
#include <pythread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include "sonyflake.h"
#include "machine_ids.h"

static int sonyflake_exec(PyObject *module);

PyModuleDef_Slot sonyflake_slots[] = {
	{Py_mod_exec, sonyflake_exec},
#if !defined(Py_LIMITED_API) || Py_LIMITED_API+0 >= 0x030c0000
	{Py_mod_multiple_interpreters, Py_MOD_PER_INTERPRETER_GIL_SUPPORTED},
#endif
#if !defined(Py_LIMITED_API) && defined(Py_GIL_DISABLED)
	{Py_mod_gil, Py_MOD_GIL_NOT_USED},
#endif
	{0, NULL}
};

struct PyModuleDef sonyflake_module = {
	PyModuleDef_HEAD_INIT,
	.m_name = MODULE_NAME,
	.m_doc = "",
	.m_size = 0,
	.m_slots = sonyflake_slots,
};

PyMODINIT_FUNC
PyInit__sonyflake(void)
{
	return PyModuleDef_Init(&sonyflake_module);
}

static int sonyflake_exec(PyObject *module) {
	PyObject *sonyflake_cls, *machine_id_lcg_cls;

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

	return 0;

err_lcg:
	Py_DECREF(machine_id_lcg_cls);
err_sf:
	Py_DECREF(sonyflake_cls);
err:
	return -1;
}
