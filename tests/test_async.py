import asyncio
from typing import Awaitable, Callable

from pytest import Subtests, mark

from sonyflake_turbo import AsyncSonyFlake, SonyFlake

AsyncSleep = Callable[[float], Awaitable[None]]
MakeNIDs = Callable[[AsyncSonyFlake, int], Awaitable[list[int]]]


async def make_n_ids_asyncgen(asf: AsyncSonyFlake, n: int) -> list[int]:
    ids: list[int] = []

    async for id_ in asf:
        if len(ids) < n:
            ids.append(id_)
        else:
            break

    return ids


async def make_n_ids_await(asf: AsyncSonyFlake, n: int) -> list[int]:
    ids: list[int] = []

    while len(ids) < n:
        ids.append(await asf)

    return ids


def make_n_ids_call(asf: AsyncSonyFlake, n: int) -> Awaitable[list[int]]:
    return asf(n)


async def _test_async_sonyflake_sub(
    sleep: AsyncSleep, make_ids: MakeNIDs, n: int
) -> None:
    sf = SonyFlake(0x0000, 0x7F7F, 0xFFFF, start_time=1749081600)
    asf = AsyncSonyFlake(sf, sleep)
    ids = await make_ids(asf, n)

    assert len(ids) == n
    assert len(set(ids)) == len(ids)
    assert sorted(ids) == ids


async def _test_async_sonyflake_main(
    subtests: Subtests, sleep: AsyncSleep, n: int
) -> None:
    with subtests.test("generator"):
        await _test_async_sonyflake_sub(sleep, make_n_ids_asyncgen, n)

    with subtests.test("await"):
        await _test_async_sonyflake_sub(sleep, make_n_ids_await, n)

    with subtests.test("call"):
        await _test_async_sonyflake_sub(sleep, make_n_ids_call, n)


@mark.parametrize("n", [1, 100, 50000], ids=["one", "no wraparound", "wraparound"])
class TestAsyncSonyFlake:
    @mark.asyncio
    async def test_asyncio(self, subtests: Subtests, n: int) -> None:
        await _test_async_sonyflake_main(subtests, asyncio.sleep, n)

    @mark.trio
    async def test_trio(self, subtests: Subtests, n: int) -> None:
        from trio import sleep

        await _test_async_sonyflake_main(subtests, sleep, n)


@mark.asyncio
async def test_with_threads() -> None:
    sf = SonyFlake(0x0000, 0x7F7F, 0xFFFF, start_time=1749081600)
    asf = AsyncSonyFlake(sf, asyncio.sleep)
    ids = []
    n = 50000

    async def async_task() -> None:
        ids.extend(await asf(n))

    def thread_task() -> None:
        ids.extend(sf(n))

    await asyncio.gather(
        asyncio.to_thread(thread_task),
        async_task(),
    )

    assert len(ids) == n * 2
    assert len(set(ids)) == len(ids)
    assert sorted(ids) == ids


@mark.asyncio
async def test_zero() -> None:
    sf = SonyFlake(0x0000, 0x7F7F, 0xFFFF, start_time=1749081600)
    asf = AsyncSonyFlake(sf, asyncio.sleep)

    assert len(await asf(0)) == 0
