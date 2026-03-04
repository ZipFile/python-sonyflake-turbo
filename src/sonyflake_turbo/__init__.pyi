from collections.abc import AsyncIterable, AsyncIterator, Awaitable
from typing import (
    Any,
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

AsyncSleep: TypeAlias = Callable[[float], Awaitable[None]]

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

    __slots__: ClassVar[tuple[str, ...]]
    sleep: AsyncSleep

    def __init__(self, sf: SonyFlake, sleep: AsyncSleep | None = None) -> None: ...
    async def __call__(self, n: int) -> list[int]: ...
    def __await__(self) -> Generator[Any, Any, int]: ...
    def __aiter__(self) -> AsyncIterator[int]: ...
    async def _gen(self) -> AsyncIterator[int]: ...
