#!/usr/bin/env python
import sysconfig
from typing import Any, Dict, List, Optional, Tuple

from setuptools import Extension, setup

options: Dict[str, Any] = {}
define_macros: List[Tuple[str, Optional[str]]] = [("_POSIX_C_SOURCE", "200809L")]
py_limited_api: bool = not sysconfig.get_config_var("Py_GIL_DISABLED")
cflags: List[str] = []

if sysconfig.get_platform().startswith("win"):
    cflags.append("/utf-8")
    cflags.append("/std:c17")
    cflags.append("/Zc:preprocessor")
    cflags.append("/experimental:c11atomics")
else:
    cflags.append("-std=c17")

if py_limited_api:
    options["bdist_wheel"] = {"py_limited_api": "cp38"}
    define_macros.append(("Py_LIMITED_API", "0x030800f0"))

setup(
    options=options,
    ext_modules=[
        Extension(
            "sonyflake_turbo._sonyflake",
            sources=[
                "src/sonyflake_turbo/_sonyflake.c",
                "src/sonyflake_turbo/sonyflake.c",
                "src/sonyflake_turbo/machine_ids.c",
            ],
            define_macros=define_macros,
            py_limited_api=py_limited_api,
            extra_compile_args=cflags,
        ),
    ],
)
