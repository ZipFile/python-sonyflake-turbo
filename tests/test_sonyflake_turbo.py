import sysconfig
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime

from pytest import mark, raises

from sonyflake_turbo import MachineIDLCG, SonyFlake


def test_no_machine_ids() -> None:
    with raises(ValueError, match=r"At least one machine ID must be provided"):
        SonyFlake()


def test_too_many_machine_ids() -> None:
    with raises(ValueError, match=r"Too many machine IDs, maximum is 65536"):
        SonyFlake(*(i for i in range(65537)))


@mark.xfail
def test_no_memory() -> None:
    with raises(MemoryError):
        SonyFlake(0x0000, 0x7F7F, 0xFFFF)


def test_non_int_machine_ids() -> None:
    with raises(TypeError, match=r"Machine IDs must be integers"):
        SonyFlake(0x0000, "0x7F7F", object())  # type: ignore[arg-type]


@mark.parametrize("machine_id", [-1, 65536])
def test_bad_machine_ids(machine_id: int) -> None:
    with raises(
        ValueError,
        match=r"Machine IDs must be in range \[0, 65535\]",
    ):
        SonyFlake(machine_id)


def test_machine_id_dupes() -> None:
    with raises(ValueError, match=r"Duplicate machine IDs are not allowed"):
        SonyFlake(0x7F7F, 0x0000, 0xFFFF, 0x7F7F)


def test_non_int_start_time() -> None:
    with raises(TypeError, match=r"start_time must be an integer"):
        SonyFlake(
            0x0000,
            0x7F7F,
            0xFFFF,
            start_time=datetime(2025, 1, 1),  # type: ignore[arg-type]
        )


@mark.parametrize(
    ["use_iter", "n"],
    [
        (use_iter, n)
        for use_iter in [True, False]
        for n in [1, 100, 250000]
    ],
)
def test_sonyflake(use_iter: bool, n: int) -> None:
    sf = SonyFlake(0x0000, 0x7F7F, 0xFFFF, start_time=1749081600)

    if use_iter:
        ids = [next(sf) for _ in range(n)]
    else:
        ids = sf(n)

    assert len(ids) == n
    assert len(set(ids)) == len(ids)
    assert sorted(ids) == ids


def test_sonyflake_repr() -> None:
    sf = SonyFlake(0x0000, 0x7F7F, 0xFFFF, start_time=1749081600)

    assert repr(sf) == "SonyFlake(0, 32639, 65535, start_time=1749081600)"


@mark.skipif(
    not sysconfig.get_config_var("Py_GIL_DISABLED"),
    reason="Requires Python with GIL disabled",
)
def test_free_threading() -> None:
    threads = 4
    out = {}
    sf = SonyFlake(0x0000, 0x7F7F, 0xFFFF, start_time=1749081600)

    def _thread(n: int, sf: SonyFlake) -> None:
        out[n] = [next(sf) for _ in range(250000)]

    with ThreadPoolExecutor(max_workers=threads) as executor:
        for n in range(threads):
            executor.submit(_thread, n, sf)

    for i in range(threads):
        ids = out[i]
        assert len(ids) == 250000
        assert len(set(ids)) == len(ids)
        assert sorted(ids) == ids


def test_machine_id_lcg() -> None:
    lcg = MachineIDLCG(123)
    ids_seq = list(range(65536))
    ids_rng = [next(lcg) for _ in range(65536)]

    assert not (set(ids_seq) - set(ids_rng))
    assert ids_seq != ids_rng


def test_machine_id_lcg_repr() -> None:
    lcg = MachineIDLCG(57243)

    assert repr(lcg) == "MachineIDLCG(57243)"
