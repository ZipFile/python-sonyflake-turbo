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

    import anyio
    from sonyflake_turbo import AsyncSonyFlake, SonyFlake

    sf = SonyFlake(0x1337, 0xCAFE, start_time=1749081600)
    asf = AsyncSonyFlake(sf, sleep=anyio.sleep)  # defaults to asyncio.sleep

    print("one", await asf)
    print("n", await asf(5))

    async for id_ in asf:
        print("aiter", id_)
        break

Important Notes
===============

Vanilla SonyFlake Difference
----------------------------

In vanilla SonyFlake, whenever counter overflows, it simply waits for the next
10ms window. Which severely limits the throughput. I.e. single generator
produces 256ids/10ms.

Turbo version is basically the same as vanilla SonyFlake, except it accepts
more than one Machine ID in constructor args. On counter overflow, it advances
to the next "unexhausted" Machine ID and resumes the generation. Waiting for
the next 10ms window happens only when all of the Machine IDs were exhausted.

This behavior is not much different from having multiple vanilla ID generators
in parallel, but by doing so we ensure produced IDs are always monotonically
increasing (per generator instance) and avoid potential concurrency issues
(by not doing concurrency).

Few other features in comparison to other SonyFlake implementations found in
the wild:

* Optional C extension module, for extra performance in CPython.
* Async-framework-agnostic wrapper.
* Thread-safe. Also has free-threading/nogil support [#]_.

.. note::

    Safe for concurrent use; internal locking ensures correctness. Sleeps are
    always done after internal state updates.

.. [#] "nogil" wheels are not published to PyPi, you have to install this
       package with ``--no-binary sonyflake-turbo`` flag.
.. _Locks: https://docs.python.org/3/library/threading.html#lock-objects

Machine IDs
-----------

Machine ID is a 16 bit integer in range ``0x0000`` to ``0xFFFF``. Machine IDs
are encoded as part of the SonyFlake ID:

+----+-----------------+------------+---------+
|    | Time            | Machine ID | Counter |
+====+=================+============+=========+
| 0x | 0874AD4993 [#]_ | CAFE       | 04      |
+----+-----------------+------------+---------+

SonyFlake IDs, in spirit, are UUIDv6_, but compressed down to 64 bit. But
unfortunately, we do not have luxury of having 48 bits for encoding node id
(UUID equivalent of SonyFlake's Machine ID). UUID standard proposes to use
pseudo-random value for this field, which is sub-optimal for our case due to
high risk of collisions.

Vanilla SonyFlake, on the other hand, used lower 16 bits of the private IP
address. Which is sort of works, but has two major drawbacks:

1. It assumes you have *exactly one* ID generator per machine in your network.
2. You're leaking some of your infrastructure info.

In the modern world (k8s, "lambdas", etc...), both of these fall apart:

1. Single machine often runs multiple different processes and/or threads.
   More often than not they're isolated enough to be able to successfully
   coordinate ID generation.
2. Security aspect aside, container IPs within cluster network are not
   something globally unique, especially if trimmed down to 16 bit.

Solving this issue is up to you, as a developer. This particular library does
not include Machine ID management logic, so you are responsible for
coordinating Machine IDs in your deployment.

Task is not trivial, but neither is impossible. Here are a few ideas:

* Coordinate ID assignment via something like etcd_ or ZooKeeper_ using lease_
  pattern. Optimal, but a bit bothersome to implement.
* Reinvent Twitter's SnowFlake_ by having a centralized service/sidecar. Extra
  round-trips SonyFlake intended to avoid.
* Assign Machine IDs manually. DevOps team will hate you.
* Use random Machine IDs. ``If I ignore it, maybe it will go away.jpg``

But nevertheless, it has one helper class: ``MachineIDLCG``. This is a
primitive LCG_-based 16 bit PRNG. It is intended to be used in tests, or in
situations where concurrency is not a problem (e.g. desktop or CLI apps).
You can also reuse it for generating IDs for a lease to avoid congestion when
going etcd/ZooKeeper route.

How many Machine IDs you want to allocate per generator is something you
should figure out on your own. Here's some numbers for you to start
(generating 1 million SonyFlake IDs):

+--------+-------------+
| Time   | Machine IDs |
+========+=============+
| 1.22s  | 32          |
+--------+-------------+
| 2.44s  | 16          |
+--------+-------------+
| 4.88s  | 8           |
+--------+-------------+
| 9.76s  | 4           |
+--------+-------------+
| 19.53s | 2           |
+--------+-------------+
| 39.06s | 1           |
+--------+-------------+

.. [#] 1409529600 + 0x874AD4993 / 100 = 2026-03-05T09:15:19.87Z
.. _UUIDv6: https://www.rfc-editor.org/rfc/rfc9562.html#name-uuid-version-6
.. _etcd: https://etcd.io/
.. _ZooKeeper: https://zookeeper.apache.org/
.. _SnowFlake: https://en.wikipedia.org/wiki/Snowflake_ID
.. _lease: https://martinfowler.com/articles/patterns-of-distributed-systems/lease.html
.. _LCG: https://en.wikipedia.org/wiki/Linear_congruential_generator

Clock Rollback
--------------

There is no logic to handle clock rollbacks or drift at the moment. If clock
moves backward, it will ``sleep()`` (``await sleep()`` in async wrapper)
until time catches up to last timestamp.

Start Time
----------

SonyFlake ID has 39 bits dedicated for the time component with a resolution of
10ms. The time is stored relative to ``start_time``. By default it is
1409529600 (``2014-09-01T00:00:00Z``), but you may want to define your own
"epoch".

Motivation
----------

Sometimes you have to bear with consequences of decisions you've made long
time ago. On a project I was leading, I made a decision to utilize SonyFlake.
Everything was fine until we needed to ingest a lot of data, very quickly.

A flame graph showed we were sleeping way too much. The culprit was
SonyFlake library we were using at that time. Some RTFM later, it was revealed
that the problem was somewhere between the chair and keyboard.

Solution was found rather quickly: just instantiate more generators and cycle
through them about every 256 IDs. Nothing could go wrong, right? Aside from
fact that hack was of questionable quality, it did work.

Except, we've got hit by `Hyrum's Law`_. Unintentional side effect of the hack
above was that IDs lost its "monotonically increasing" property [#]_. Ofc, some
of our and other team's code were dependent on this SonyFlake's feature. Duh.

Adding even more workarounds like pre-generate IDs, sort them and ingest was
a compelling idea, but I did not feel right. Hence, this library was born.

.. [#] E.g. if you cycle through generators with Machine IDs 0xCAFE and 0x1337
       You may get the following IDs: ``0x0874b2a7a0cafe00``,
       ``0x0874b2a7a0133700``. Even though there are no collisions, sorting
       them will result in a different order (vs order they've been generated)
.. _Hyrum's Law: https://www.hyrumslaw.com/

Why should I use it?
--------------------

If you're starting a new project, please use UUIDv7_. It is superior to
SonyFlake in almost every way. It is an internet standard (RFC 9562), it is
already available in various languages' standard libraries and is supported by
popular databases (PostgreSQL, MariaDB, etc...).

Otherwise you might want to use it for one of the following reasons:

* You already use it and encountered similar problems mentioned in
  `Motivation`_ section.
* You want to avoid extra round-trips to fetch IDs.
* Usage of UUIDs is not feasible (legacy codebase, db indexes limited to 64
  bit integers, etc...) but you still want to benefit from index
  locality/strict global ordering.
* As a cheap way to reduce predicability of IDOR_ attacks.
* Architecture lunatism is still strong within you and you want your code to
  be DDD-like (e.g. being able to reference an entity before it is stored in
  DB).

.. _UUIDv7: https://www.rfc-editor.org/rfc/rfc9562.html#name-uuid-version-7
.. _IDOR: https://cheatsheetseries.owasp.org/cheatsheets/Insecure_Direct_Object_Reference_Prevention_Cheat_Sheet.html

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

    pytest

Build wheels:

.. code-block:: sh

    pip install cibuildwheel
    cibuildwheel

Build a ``py3-none-any`` wheel (without the C extension):

.. code-block:: sh

    SONYFLAKE_TURBO_BUILD=0 python -m build --wheel
