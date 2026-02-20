#include <Python.h>
#include <pythread.h>
#include <stdbool.h>
#include <stdint.h>

#include "module.h"
#include "sonyflake.h"
#include "machine_ids.h"

static int sonyflake_module_traverse(PyObject *m, visitproc visit, void *arg) {
	struct sonyflake_module_state *state = PyModule_GetState(m);
	Py_VISIT(state->sonyflake_cls);
	Py_VISIT(state->machine_id_lcg_cls);
	return 0;
}

static int sonyflake_module_clear(PyObject *m) {
	struct sonyflake_module_state *state = PyModule_GetState(m);
	Py_CLEAR(state->sonyflake_cls);
	Py_CLEAR(state->machine_id_lcg_cls);
	return 0;
}

static void sonyflake_module_free(void *m) {
	sonyflake_module_clear(m);
}

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
	.m_size = sizeof(struct sonyflake_module_state),
	.m_slots = sonyflake_slots,
	.m_traverse = sonyflake_module_traverse,
	.m_clear = sonyflake_module_clear,
	.m_free = sonyflake_module_free,
};

PyMODINIT_FUNC
PyInit__sonyflake(void)
{
	return PyModuleDef_Init(&sonyflake_module);
}

static int sonyflake_exec(PyObject *module) {
	struct sonyflake_module_state *state = PyModule_GetState(module);

	state->sonyflake_cls = NULL;
	state->machine_id_lcg_cls = NULL;

	state->sonyflake_cls = PyType_FromModuleAndSpec(module, &sonyflake_type_spec, NULL);

	if (!state->sonyflake_cls) {
		goto err;
	}

	if (PyModule_AddObjectRef(module, "SonyFlake", state->sonyflake_cls) < 0) {
		goto err;
	}

	state->machine_id_lcg_cls = PyType_FromModuleAndSpec(module, &machine_id_lcg_spec, NULL);

	if (!state->machine_id_lcg_cls) {
		goto err;
	}

	if (PyModule_AddObjectRef(module, "MachineIDLCG", state->machine_id_lcg_cls) < 0) {
		goto err;
	}

	PyModule_AddIntMacro(module, SONYFLAKE_EPOCH);
	PyModule_AddIntMacro(module, SONYFLAKE_SEQUENCE_BITS);
	PyModule_AddIntMacro(module, SONYFLAKE_SEQUENCE_MAX);
	PyModule_AddIntMacro(module, SONYFLAKE_MACHINE_ID_BITS);
	PyModule_AddIntMacro(module, SONYFLAKE_MACHINE_ID_MAX);
	PyModule_AddIntMacro(module, SONYFLAKE_MACHINE_ID_OFFSET);
	PyModule_AddIntMacro(module, SONYFLAKE_TIME_OFFSET);

	return 0;

err:
	Py_CLEAR(state->machine_id_lcg_cls);
	Py_CLEAR(state->sonyflake_cls);

	return -1;
}
