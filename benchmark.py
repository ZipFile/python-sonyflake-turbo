#!/usr/bin/env python3
import platform
import sys
from asyncio import new_event_loop, set_event_loop
from contextlib import suppress
from datetime import datetime, timezone
from timeit import timeit

from sonyflake_turbo import AsyncSonyFlake

EPOCH = 1749081600
EPOCH_DT = datetime.fromtimestamp(EPOCH, tz=timezone.utc)
ONE_THOUSAND = 1000
ONE_MILLION = ONE_THOUSAND * ONE_THOUSAND
# Benchmarking a single id generator is not useful since it'll be bottlenecked
# by SonyFlake's own throughtput. Use all possible machine ids to alleviate
# stray sleeps.
MACHINE_IDS = range(0xFFFF)
# `asyncio.run()` creates new event loop on every invocation, which is no-go
# for benchmarking. Reuse the same event loop for all benchmarks.
loop = new_event_loop()
set_event_loop(loop)


def _turbo(name, sf):
    t_batch = timeit(lambda: sf(ONE_THOUSAND), number=ONE_THOUSAND)
    t_solo = timeit(lambda: next(sf), number=ONE_MILLION)

    print(f"turbo_{name}_batch,{t_batch:.2f}")
    print(f"turbo_{name}_solo,{t_solo:.2f}")


def _turbo_async(name, asf):
    def batch():
        loop.run_until_complete(asf(ONE_THOUSAND))

    def solo():
        loop.run_until_complete(asf)

    t_batch = timeit(batch, number=ONE_THOUSAND)
    t_solo = timeit(solo, number=ONE_MILLION)

    print(f"turbo_{name}_batch_async,{t_batch:.2f}")
    print(f"turbo_{name}_solo_async,{t_solo:.2f}")


def _benchmark(name, sfs, aio=False):
    max_i = len(sfs)
    i = 0

    if aio:
        name += "_async"

        def next_id() -> int:
            nonlocal i
            id_ = loop.run_until_complete(sfs[i]())
            i = (i + 1) % max_i
            return id_

    else:

        def next_id() -> int:
            nonlocal i
            id_ = sfs[i]()
            i = (i + 1) % max_i
            return id_

    t = timeit(next_id, number=ONE_MILLION)

    print(f"{name},{t:.2f}")


def turbo_native():
    from sonyflake_turbo._sonyflake import SonyFlake

    _turbo("native", SonyFlake(*MACHINE_IDS, start_time=EPOCH))


def turbo_pure():
    from sonyflake_turbo.pure import SonyFlake

    _turbo("pure", SonyFlake(*MACHINE_IDS, start_time=EPOCH))


def turbo_async_native():
    from sonyflake_turbo._sonyflake import SonyFlake

    asf = AsyncSonyFlake(SonyFlake(*MACHINE_IDS, start_time=EPOCH))

    _turbo_async("native", asf)


def turbo_async_pure():
    from sonyflake_turbo.pure import SonyFlake

    asf = AsyncSonyFlake(SonyFlake(*MACHINE_IDS, start_time=EPOCH))

    _turbo_async("pure", asf)


def hjpotter92_sonyflake():
    """pip install sonyflake-py"""

    from sonyflake import SonyFlake

    sfs = [
        SonyFlake(start_time=EPOCH_DT, machine_id=lambda: machine_id).next_id
        for machine_id in MACHINE_IDS
    ]

    _benchmark("hjpotter92_sonyflake", sfs)


def iyad_f_sonyflake():
    """pip install sonyflake"""

    from sonyflake import Sonyflake

    sfs = [
        Sonyflake(start_time=EPOCH_DT, machine_id=machine_id).next_id
        for machine_id in MACHINE_IDS
    ]

    _benchmark("iyad_f_sonyflake", sfs)


def iyad_f_sonyflake_async():
    """pip install sonyflake"""

    from sonyflake import Sonyflake

    sfs = [
        Sonyflake(start_time=EPOCH_DT, machine_id=machine_id).next_id_async
        for machine_id in MACHINE_IDS
    ]

    _benchmark("iyad_f_sonyflake", sfs, True)


def snowflake_id_toolkit():
    from snowflake_id_toolkit.sony import SonyflakeIDGenerator

    # https://github.com/pavelprokhorenko/snowflake-id-toolkit/issues/19
    object.__setattr__(SonyflakeIDGenerator._config, "node_id_bits", 16)
    object.__setattr__(SonyflakeIDGenerator._config, "sequence_bits", 8)

    sfs = [
        SonyflakeIDGenerator(epoch=EPOCH, node_id=machine_id).generate_next_id
        for machine_id in MACHINE_IDS
    ]

    _benchmark("snowflake_id_toolkit", sfs)


def main():
    print(platform.python_implementation(), platform.python_version(), file=sys.stderr)
    for f in [
        turbo_native,
        turbo_pure,
        turbo_async_native,
        turbo_async_pure,
        hjpotter92_sonyflake,
        iyad_f_sonyflake,
        iyad_f_sonyflake_async,
        snowflake_id_toolkit,
    ]:
        with suppress(ImportError):
            f()
    print("done", file=sys.stderr)


if __name__ == "__main__":
    main()
    # CPython 3.12.3
    # turbo_native_batch,0.03
    # turbo_native_solo,0.13
    # turbo_pure_batch,0.35
    # turbo_pure_solo,0.99
    # turbo_native_batch_async,0.05
    # turbo_native_solo_async,10.16
    # turbo_pure_batch_async,0.31
    # turbo_pure_solo_async,12.23
    # hjpotter92_sonyflake,1.35
    # iyad_f_sonyflake,2.48
    # iyad_f_sonyflake_async,14.07
    # snowflake_id_toolkit,1.14
