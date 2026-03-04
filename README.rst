========================
Python SonyFlake (Turbo)
========================

A `SonyFlake <https://github.com/sony/sonyflake>`_ ID generator tailored for
high-volume ID generation.

Installation
============

.. code-block:: sh

    pip install sonyflake-turbo

Usage
=====

Easy mode:

.. code-block:: python

    from sonyflake_turbo import SonyFlake

    sf = SonyFlake(0x1337, 0xCAFE, start_time=1749081600)

    print("one", next(sf))
    print("n", sf(5))

    for id_ in sf:
        print("iter", id_)
        break

Turbo mode:

.. code-block:: python

    from time import time_ns
    from timeit import timeit

    from sonyflake_turbo import MachineIDLCG, SonyFlake

    get_machine_id = MachineIDLCG(time_ns())
    EPOCH = 1749081600  # 2025-06-05T00:00:00Z

    for count in [32, 16, 8, 4, 2, 1]:
        machine_ids = [get_machine_id() for _ in range(count)]
        sf = SonyFlake(*machine_ids, start_time=EPOCH)
        t = timeit(lambda: sf(1000), number=1000)
        print(f"Speed: 1M ids / {t:.2f}sec with {count} machine IDs")

Async:

.. code-block:: python

    from anyio import sleep  # AsyncSonyFlake supports both asyncio and trio
    from sonyflake_turbo import AsyncSonyFlake, SonyFlake

    sf = SonyFlake(0x1337, 0xCAFE, start_time=1749081600)
    asf = AsyncSonyFlake(sf, sleep)

    print("one", await asf)
    print("n", await asf(5))

    async for id_ in asf:
        print("aiter", id_)
        break

Important Notes
===============

SonyFlake algorithm produces IDs at rate 256 IDs per 10msec per 1 Machine ID.
One obvious way to increase the throughput is to use multiple generators with
different Machine IDs. This library provides a way to do exactly that by
passing multiple Machine IDs to the constructor of the `SonyFlake` class.
Generated IDs are non-repeating and are always increasing. But be careful! You
should be conscious about assigning Machine IDs to different processes and/or
machines to avoid collisions. This library does not come with any Machine ID
management features, so it's up to you to figure this out.

This library has limited free-threaded mode support. It won't crash, but
you won't get much performance gain from multithreaded usage. Consider
creating generators per thread instead of sharing them across multiple
threads.

This library also contains pure-Python implementation as a fallback in case of
C extension unavailability (e.g. with PyPy or when installed with
``--no-binary`` flag).

Development
===========

Install:

.. code-block:: sh

    python3 -m venv env
    . env/bin/activate
    pip install -r requirements-dev.txt
    pip install -e .

Run tests:

.. code-block:: sh

    py.test

Building wheels:

.. code-block:: sh

    pip install cibuildwheel
    cibuildwheel

Building ``py3-none-any`` wheel (without C extension):

.. code-block:: sh

    SONYFLAKE_TURBO_BUILD=0 python -m build --wheel
