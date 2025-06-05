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

    sf = SonyFlake(0x1337, 0xCAFE)

    for _, id_ in zip(range(10), sf):
        print(f"{id_:016x}")

Turbo mode:

.. code-block:: python

    from datetime import datetime, timezone
    from random import sample
    from timeit import timeit

    from sonyflake_turbo import SONYFLAKE_MACHINE_ID_MAX, SonyFlake

    epoch = datetime(2025, 6, 5, tzinfo=timezone.utc)

    for count in [32, 16, 8, 4, 2, 1]:
        machine_ids = sample(range(SONYFLAKE_MACHINE_ID_MAX + 1), count)
        sf = SonyFlake(*machine_ids, start_time=int(epoch.timestamp()))
        t = timeit(lambda: [next(sf) for _ in range(1000)], number=1000)
        print(f"Speed: 1M ids / {t:.2f}sec with {count} machine IDs")

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

Development
===========

Install:

.. code-block:: sh

    python3 -m venv env
    . env/bin/activate
    pip install -e .[dev]

Run tests:

.. code-block:: sh

    py.test

Building wheels:

.. code-block:: sh

    pip install cibuildwheel
    cibuildwheel
