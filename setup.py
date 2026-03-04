#!/usr/bin/env python
import os
import sys
import sysconfig
from typing import Any, Dict, List, Optional, Tuple

from setuptools import Extension, setup


def get_bool(name: str, default: bool = False) -> bool:
    try:
        return os.environ[f"SONYFLAKE_TURBO_{name}"].lower() in ("1", "true")
    except KeyError:
        return default


build: bool = get_bool("BUILD", True)
build_required: bool = get_bool("BUILD_REQUIRED", False)
options: Dict[str, Any] = {}
define_macros: List[Tuple[str, Optional[str]]] = []
py_limited_api: bool = not sysconfig.get_config_var("Py_GIL_DISABLED")
cflags: List[str] = []

if sys.implementation.name != "cpython":
    build = False

if sysconfig.get_platform().startswith("win"):
    cflags.append("/utf-8")
    cflags.append("/std:c17")
    cflags.append("/Zc:preprocessor")
    cflags.append("/experimental:c11atomics")
else:
    cflags.append("-std=c17")
    cflags.append("-Wall")
    cflags.append("-Wextra")
    cflags.append("-Werror")

if py_limited_api:
    options["bdist_wheel"] = {"py_limited_api": "cp310"}
    define_macros.append(("Py_LIMITED_API", "0x030a00f0"))


setup_kwargs = {
    "options": options,
    "ext_modules": [
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
            optional=not build_required,
        ),
    ],
}

if not build:
    setup_kwargs = {}

setup(**setup_kwargs)
