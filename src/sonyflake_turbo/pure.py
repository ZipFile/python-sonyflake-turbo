from threading import Lock
from time import sleep, time, time_ns
from typing import overload

try:
    from typing import Self
except ImportError:  # pragma: no cover
    from typing_extensions import Self


SONYFLAKE_EPOCH = 1409529600  # 2014-09-01 00:00:00 UTC
SONYFLAKE_SEQUENCE_BITS = 8
SONYFLAKE_SEQUENCE_COUNT = 1 << SONYFLAKE_SEQUENCE_BITS
SONYFLAKE_SEQUENCE_MAX = SONYFLAKE_SEQUENCE_COUNT - 1
SONYFLAKE_MACHINE_ID_BITS = 16
SONYFLAKE_MACHINE_ID_MAX = (1 << SONYFLAKE_MACHINE_ID_BITS) - 1
SONYFLAKE_MACHINE_ID_OFFSET = SONYFLAKE_SEQUENCE_BITS
SONYFLAKE_TIME_OFFSET = SONYFLAKE_MACHINE_ID_BITS + SONYFLAKE_SEQUENCE_BITS


def sf_to_ns(start_time: int, sf: int) -> int:
    return (start_time + sf) * 10_000_000


def ns_to_sf(start_time: int, ns: int) -> int:
    return ns // 10_000_000 - start_time


def compose(i: int, elapsed: int, machine_ids: list[int]) -> int:
    t = elapsed << SONYFLAKE_TIME_OFFSET
    m = machine_ids[i >> SONYFLAKE_SEQUENCE_BITS] << SONYFLAKE_MACHINE_ID_OFFSET
    c = i & SONYFLAKE_SEQUENCE_MAX
    return t | m | c


def diff(start_time: int, elapsed: int, current_ns: int) -> float:
    d = sf_to_ns(start_time, elapsed) - current_ns
    if d > 0:
        return d / 1_000_000_000
    return 0


def machine_id_lcg(x: int) -> int:
    return (32309 * x + 13799) % 65536


class SonyFlake:
    __slots__ = ("_machine_ids", "_start_time", "_elapsed", "_max_i", "_i", "_lock")

    _start_time: int
    _elapsed: int
    _machine_ids: list[int]
    _max_i: int
    _i: int
    _lock: Lock

    def __init__(self, *machine_ids: int, start_time: int = SONYFLAKE_EPOCH):
        machine_ids_set = set(machine_ids)

        if len(machine_ids_set) == 0:
            raise ValueError("At least one machine ID must be provided")

        if len(machine_ids_set) > 65536:
            raise ValueError("Too many machine IDs, maximum is 65536")

        if len(machine_ids_set) != len(machine_ids):
            raise ValueError("Duplicate machine IDs are not allowed")

        if not all(map(lambda x: isinstance(x, int), machine_ids_set)):
            raise TypeError("Machine IDs must be integers")

        if min(machine_ids_set) < 0 or max(machine_ids_set) > 0xFFFF:
            raise ValueError("Machine IDs must be in range [0, 65535]")

        if not isinstance(start_time, int):
            raise TypeError("start_time must be an integer")

        if start_time >= time():
            raise ValueError("start_time must be in the past")

        self._machine_ids = sorted(machine_ids_set)
        self._max_i = len(machine_ids_set) * SONYFLAKE_SEQUENCE_COUNT
        self._start_time = start_time * 100
        self._elapsed = 0
        self._i = 0
        self._lock = Lock()

    def __iter__(self) -> Self:
        return self

    def __next__(self) -> int:
        id_, to_sleep = self._next()
        if to_sleep > 0:
            sleep(to_sleep)
        return id_

    def __call__(self, n: int) -> list[int]:
        ids, to_sleep = self._next_n(n)
        sleep(to_sleep)
        return ids

    def __repr__(self) -> str:
        cls = self.__class__.__name__
        machine_ids = ", ".join(map(str, self._machine_ids))
        return f"{cls}({machine_ids}, start_time={self._start_time // 100})"

    @overload
    def _raw(self, n: int) -> tuple[list[int], float]: ...
    @overload
    def _raw(self, n: None) -> tuple[int, float]: ...

    def _raw(self, n: int | None) -> tuple[int | list[int], float]:
        if n is None:
            return self._next()
        return self._next_n(n)

    def _next(self) -> tuple[int, float]:
        start_time = self._start_time
        machine_ids = self._machine_ids

        with self._lock:
            current_ns = time_ns()
            current = ns_to_sf(start_time, current_ns)
            elapsed = self._elapsed

            if elapsed < current:
                self._elapsed = elapsed = current
                self._i = i = 0
            else:
                self._i = i = (self._i + 1) % self._max_i

                if i == 0:
                    self._elapsed = elapsed = elapsed + 1

        return (
            compose(i, elapsed, machine_ids),
            diff(start_time, elapsed, current_ns),
        )

    def _next_n(self, n: int) -> tuple[list[int], float]:
        if n < 1:
            return [], 0

        start_time = self._start_time
        machine_ids = self._machine_ids
        max_i = self._max_i
        result: list[int] = []

        with self._lock:
            current_ns = time_ns()
            current = ns_to_sf(start_time, current_ns)
            elapsed = self._elapsed

            if elapsed < current:
                elapsed = current
                i = 0
            else:
                i = self._i

            while len(result) < n:
                result.append(compose(i, elapsed, machine_ids))

                i = (i + 1) % max_i

                if i == 0:
                    elapsed += 1

            self._elapsed = elapsed
            self._i = i

        return result, diff(start_time, elapsed, current_ns)


class MachineIDLCG:
    def __init__(self, seed: int = 0):
        self._seed = seed
        self._lock = Lock()

    def __next__(self) -> int:
        with self._lock:
            self._seed = x = machine_id_lcg(self._seed)
            return x

    def __repr__(self) -> str:
        return f"{self.__class__.__name__}({self._seed})"
