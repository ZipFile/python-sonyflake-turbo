============
Benchmarking
============

As of Mar 2026, there are 3 alternative implementations of SonyFlake in
Python:

* `hjpotter92/sonyflake-py <https://github.com/hjpotter92/sonyflake-py>`_
* `iyad-f/sonyflake <https://github.com/iyad-f/sonyflake>`_
* `pavelprokhorenko/snowflake-id-toolkit <https://github.com/pavelprokhorenko/snowflake-id-toolkit>`_

None of them has batch generation feature, neither they use extension modules
for speed ups. So we'll be comparing against pure-Python, single-id generation
mode in ``sonyflake-turbo``:

.. code-block:: python

    sf = SonyFlake(*range(0xFFFF))
    asf = AsyncSonyFlake(sf)
    next(sf)  # for sync benchmarks
    await asf  # for async benchmarks

Since Sonyflake algorithm at its core has limited throughput, we'll be using
as much machine ids as possible. For ``sonyflake-turbo`` it is as trivial
as:

.. code-block:: python

    sf = SonyFlake(*range(0xFFFF))

for other implementation we'll be using something equivalent to:

.. code-block:: python

    sfs = [
        SonyFlake(machine_id=machine_id).next_id
        for machine_id in range(0xFFFF)
    ]
    max_i = len(sfs)
    i = 0

    def next_id() -> int:
        nonlocal i
        id_ = sfs[i]()
        i = (i + 1) % max_i
        return id_


In order to shave-off some time for ``getattr``-ing of ``next_id`` we do it
in initialization step. Same goes for ``len``-s. We're not using generators,
since ``yield``-s are also tanking performance.

This setup is closest to "running multiple generators in parallel".

Unfortunately, both ``hjpotter92/sonyflake-py`` and ``iyad-f/sonyflake``
share the same package name ``sonyflake``, so we cannot benchmark everything
in one go. Results has to be merged later.

Full source code of benchmark is located in ``benchmark.py`` file in the root
of the repo.

Naming breakdown:

* ``hjpotter92_sonyflake`` - ``hjpotter92/sonyflake-py``
* ``iyad_f_sonyflake*`` - ``iyad-f/sonyflake``
* ``snowflake_id_toolkit`` - ``pavelprokhorenko/snowflake-id-toolkit``
* ``oneflake`` - ``felipediel/oneflake``
* ``turbo_native*`` - running against ``sonyflake_turbo._sonyflake.SonyFlake``
* ``turbo_pure*`` - running against ``sonyflake_turbo.pure.SonyFlake``
* ``turbo_*_solo`` - solo mode (``next(sf)``, ``await asf``)
* ``turbo_*_batch`` - batch mode (``sf(1000)``, ``await asf(1000)``)

Run:

.. code-block:: sh

    python benchmark.py

Results
=======

For CPython 3.12.3 on the Intel Xeon E3-1275, results are following (lower %
= better):

.. csv-table:: Sync
    :header: "Name", "Time", "%"

    turbo_native_batch,0.03s,3.03%
    turbo_native_solo,0.13s,13.13%
    turbo_pure_batch,0.35s,35.35%
    turbo_pure_solo,0.99s,100.00%
    hjpotter92_sonyflake,1.35s,136.36%
    iyad_f_sonyflake,2.48s,250.51%
    snowflake_id_toolkit,1.14s,115.15%
    oneflake,0.16s,16.16%

.. csv-table:: Async
    :header: "Name", "Time", "%"

    turbo_native_batch_async,0.05s,0.41%
    turbo_native_solo_async,10.16s,83.07%
    turbo_pure_batch_async,0.31s,2.53%
    turbo_pure_solo_async,12.23s,100.00%
    iyad_f_sonyflake_async,14.07s,115.04%
