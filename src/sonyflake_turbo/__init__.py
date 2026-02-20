from typing import (
    Any,
    AsyncIterable,
    AsyncIterator,
    Awaitable,
    Callable,
    ClassVar,
    Generator,
    TypeAlias,
)

from ._sonyflake import (
    SONYFLAKE_EPOCH,
    SONYFLAKE_MACHINE_ID_BITS,
    SONYFLAKE_MACHINE_ID_MAX,
    SONYFLAKE_MACHINE_ID_OFFSET,
    SONYFLAKE_SEQUENCE_BITS,
    SONYFLAKE_SEQUENCE_MAX,
    SONYFLAKE_TIME_OFFSET,
    MachineIDLCG,
    SonyFlake,
)

AsyncSleep: TypeAlias = Callable[[float], Awaitable[None]]

__all__ = [
    "SONYFLAKE_EPOCH",
    "SONYFLAKE_MACHINE_ID_BITS",
    "SONYFLAKE_MACHINE_ID_MAX",
    "SONYFLAKE_MACHINE_ID_OFFSET",
    "SONYFLAKE_SEQUENCE_BITS",
    "SONYFLAKE_SEQUENCE_MAX",
    "SONYFLAKE_TIME_OFFSET",
    "AsyncSonyFlake",
    "MachineIDLCG",
    "SonyFlake",
]


class AsyncSonyFlake(Awaitable[int], AsyncIterable[int]):
    """Async wrapper for :class:`SonyFlake`.

    Usage:

        .. code-block:: python

            import asyncio

            sf = SonyFlake(0x1337, 0xCAFE, start_time=1749081600)
            asf = AsyncSonyFlake(sf, asyncio.sleep)

            print(await asf)
            print(await asf(5))

            async for id_ in asf:
                print(id_)
                break
    """

    __slots__: ClassVar[tuple[str, ...]] = ("sf", "sleep")
    sleep: AsyncSleep

    def __init__(self, sf: SonyFlake, sleep: AsyncSleep | None = None) -> None:
        """Initialize AsyncSonyFlake ID generator.

        Args:
            sf: Instance of the :class:`SonyFlake`.
            sleep: Either `asyncio.sleep` or `trio.sleep`.
        """

        if sleep is None:
            from asyncio import sleep

            assert sleep is not None

        self.sf = sf
        self.sleep = sleep

    async def __call__(self, n: int) -> list[int]:
        """Generate multiple SonyFlake IDs at once.

        Roughly equivalent to ``[await asf for _ in range(n)]``, but more
        efficient. This method saves on task/context switches and syscalls for
        getting current time.

        Args:
            n: Number of ids to generate.

        Returns:
            List of ids.
        """

        ids, to_sleep = self.sf._raw(n)

        await self.sleep(to_sleep)

        return ids

    def __await__(self) -> Generator[Any, Any, int]:
        """Produce a SonyFlake ID."""

        id_, to_sleep = self.sf._raw(None)

        yield from self.sleep(to_sleep).__await__()

        return id_

    def __aiter__(self) -> AsyncIterator[int]:
        """Return an infinite SonyFlake ID async iterator."""

        return self._gen().__aiter__()

    async def _gen(self) -> AsyncIterator[int]:
        """Infinite SonyFlake ID async generator."""

        while True:
            id_, to_sleep = self.sf._raw(None)
            await self.sleep(to_sleep)
            yield id_
