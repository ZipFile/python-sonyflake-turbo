import signal
import sysconfig
import threading
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime
from time import perf_counter, sleep, time

from pytest import mark, raises

from sonyflake_turbo._sonyflake import MachineIDLCG as CMachineIDLCG
from sonyflake_turbo._sonyflake import SonyFlake as CSonyFlake
from sonyflake_turbo.pure import MachineIDLCG as PyMachineIDLCG
from sonyflake_turbo.pure import SonyFlake as PySonyFlake

START_TIME = 1749081600


@mark.parametrize("cls", [CMachineIDLCG, PyMachineIDLCG], ids=["c", "py"])
class TestMachineIDLCG:
    def test_machine_id_lcg(self, cls: type) -> None:
        lcg = cls(123)
        ids_seq = list(range(65536))
        ids_rng = [next(lcg) for _ in range(65536)]

        assert not (set(ids_seq) - set(ids_rng))
        assert ids_seq != ids_rng

    def test_machine_id_lcg_repr(self, cls: type) -> None:
        assert repr(cls(57243)) == "MachineIDLCG(57243)"


@mark.parametrize("cls", [CSonyFlake, PySonyFlake], ids=["c", "py"])
class TestSonyFlake:
    def test_no_machine_ids(self, cls: type) -> None:
        with raises(ValueError, match=r"At least one machine ID must be provided"):
            cls()

    def test_too_many_machine_ids(self, cls: type) -> None:
        with raises(ValueError, match=r"Too many machine IDs, maximum is 65536"):
            cls(*(i for i in range(65537)))

    def test_non_int_machine_ids(self, cls: type) -> None:
        with raises(TypeError, match=r"Machine IDs must be integers"):
            cls(0x0000, "0x7F7F", object())

    @mark.parametrize("machine_id", [-1, 65536])
    def test_bad_machine_ids(self, machine_id: int, cls: type) -> None:
        with raises(
            ValueError,
            match=r"Machine IDs must be in range \[0, 65535\]",
        ):
            cls(machine_id)

    def test_machine_id_dupes(self, cls: type) -> None:
        with raises(ValueError, match=r"Duplicate machine IDs are not allowed"):
            cls(0x7F7F, 0x0000, 0xFFFF, 0x7F7F)

    def test_non_int_start_time(self, cls: type) -> None:
        with raises(TypeError, match=r"start_time must be an integer"):
            cls(
                0x0000,
                0x7F7F,
                0xFFFF,
                start_time=datetime(2025, 1, 1),
            )

    def test_start_time_in_future(self, cls: type) -> None:
        with raises(ValueError, match=r"start_time must be in the past"):
            cls(0x0000, start_time=int(time() + 10))

    @mark.parametrize(
        ["use_iter", "n"],
        [(use_iter, n) for use_iter in [True, False] for n in [1, 100, 250000]],
    )
    def test_sonyflake(self, use_iter: bool, n: int, cls: type) -> None:
        sf = cls(0x0000, 0x7F7F, 0xFFFF, start_time=START_TIME)

        if use_iter:
            it = iter(sf)
            ids = [next(it) for _ in range(n)]
        else:
            ids = sf(n)

        assert len(ids) == n
        assert len(set(ids)) == len(ids)
        assert sorted(ids) == ids

    def test_sonyflake_n_twice(self, cls: type) -> None:
        sf = cls(0x0000, 0x7F7F, 0xFFFF, start_time=START_TIME)
        ids = [*sf(2), *sf(2)]

        assert len(ids) == 4
        assert len(set(ids)) == len(ids)
        assert sorted(ids) == ids

    @mark.parametrize("n", [-1, 0])
    def test_sonyflake_zero(self, n: int, cls: type) -> None:
        assert cls(0x0000)(0) == []

    def test_sonyflake_repr(self, cls: type) -> None:
        sf = cls(0x0000, 0x7F7F, 0xFFFF, start_time=START_TIME)

        assert repr(sf) == "SonyFlake(0, 32639, 65535, start_time=1749081600)"

    def test_raw_list(self, cls: type) -> None:
        sf = cls(0x0000, 0x7F7F, 0xFFFF, start_time=START_TIME)
        ids, to_sleep = sf._raw(250000)
        assert len(ids) == 250000
        assert to_sleep > 0

    def test_raw_none(self, cls: type) -> None:
        sf = cls(0x0000, 0x7F7F, 0xFFFF, start_time=START_TIME)
        id_, to_sleep = sf._raw(None)
        assert isinstance(id_, int)
        assert to_sleep == 0


@mark.skipif(
    not hasattr(signal, "pthread_kill"),
    reason="pthread_kill not supported",
)
def test_c_sonyflake_sigint() -> None:
    sf = CSonyFlake(0x0000, start_time=START_TIME)
    thread_id = threading.get_ident()

    def thread_func() -> None:
        sleep(0.1)
        signal.pthread_kill(thread_id, signal.SIGINT)

    threading.Thread(target=thread_func).start()

    with raises(KeyboardInterrupt):
        sf(50000)


@mark.skipif(
    not hasattr(signal, "pthread_kill"),
    reason="pthread_kill not supported",
)
def test_c_sonyflake_sigusr() -> None:
    sf = CSonyFlake(0x0000, start_time=START_TIME)
    thread_id = threading.get_ident()

    signal.signal(signal.SIGUSR1, lambda *_: None)

    def thread_func() -> None:
        sleep(0.1)
        signal.pthread_kill(thread_id, signal.SIGUSR1)

    threading.Thread(target=thread_func).start()
    a = perf_counter()
    ids = sf(50000)
    b = perf_counter()

    assert len(ids) == 50000
    assert (b - a) > 1  # should take roughly 1.9sec


@mark.skipif(
    not sysconfig.get_config_var("Py_GIL_DISABLED"),
    reason="Requires Python with GIL disabled",
)
def test_c_free_threading() -> None:
    threads = 4
    out = {}
    sf = CSonyFlake(0x0000, 0x7F7F, 0xFFFF, start_time=START_TIME)

    def _thread(n: int, sf: CSonyFlake) -> None:
        out[n] = [next(sf) for _ in range(250000)]

    with ThreadPoolExecutor(max_workers=threads) as executor:
        for n in range(threads):
            executor.submit(_thread, n, sf)

    for i in range(threads):
        ids = out[i]
        assert len(ids) == 250000
        assert len(set(ids)) == len(ids)
        assert sorted(ids) == ids
