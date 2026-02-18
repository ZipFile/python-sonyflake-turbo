from typing import Awaitable, Callable

from pytest import mark

from sonyflake_turbo._sonyflake import sleep_wrapper

AsyncSleep = Callable[[float], Awaitable[None]]
obj = object()
to_sleep = 0.1


async def _test_sleep_wrapper(sleep: AsyncSleep) -> None:
    sleep_called_with = None

    def _sleep(t: float) -> Awaitable[None]:
        nonlocal sleep_called_with
        sleep_called_with = t
        return sleep(t)

    assert await sleep_wrapper(obj, _sleep, to_sleep) is obj
    assert sleep_called_with is to_sleep, "sleep not called"


@mark.asyncio
async def test_asyncio_sleep() -> None:
    from asyncio import sleep

    await _test_sleep_wrapper(sleep)


@mark.trio
async def test_trio_gen() -> None:
    from trio import sleep

    await _test_sleep_wrapper(sleep)
