from typing import Awaitable, Callable, Generator, List, Optional, TypeAlias, TypeVar

try:
    from typing import Self
except ImportError:
    from typing_extensions import Self

SONYFLAKE_EPOCH: int
SONYFLAKE_SEQUENCE_BITS: int
SONYFLAKE_SEQUENCE_MAX: int
SONYFLAKE_MACHINE_ID_BITS: int
SONYFLAKE_MACHINE_ID_MAX: int
SONYFLAKE_MACHINE_ID_OFFSET: int
SONYFLAKE_TIME_OFFSET: int
AsyncSleep: TypeAlias = Callable[[float], Awaitable[None]]
T = TypeVar("T")

async def sleep_wrapper(obj: T, sleep: AsyncSleep, to_sleep: float) -> T:
    """C version of:

    async def sleep_wrapper(obj, sleep, to_sleep):
        await sleep(to_sleep)
        return obj
    """

class SonyFlake:
    def __init__(self, *machine_id: int, start_time: Optional[int] = None):
        """Initialize SonyFlake ID generator.

        Args:
            machine_id: Unique ID(s) of a SonyFlake instance.
                A number in range [0, 0xFFFF]. Must be provided at least one,
                at most 65536, and have no duplicates.
            start_time: Time since which the SonyFlake time is defined as the
                elapsed time. A UNIX timestamp in UTC time zone, if unset
                defaults to 1409529600 (2014-09-01 00:00:00 UTC). Must be in
                the past.

        Raises:
            ValueError: Invalid values of ``machine_id`` or ``start_time``.
            TypeError: ``machine_id`` or ``start_time`` are not integers.
        """

    def __iter__(self) -> Self:
        """Returns ``self``."""

    def __next__(self) -> int:
        """Produce a SonyFlake ID."""

    def __call__(self, n: int, /) -> List[int]:
        """Generate multiple SonyFlake IDs at once.

        Roughly equivalent to `[next(sf) for _ in range(n)]`, but more
        efficient. This method saves on syscalls to sleep and getting current
        time.

        Important:
            The more ids you request, the more other threads have to wait
            upon the next id(s).

        Args:
            n: Number of ids to generate. Must be greater than 0.

        Raises:
            ValueError: if n <= 0

        Returns:
            List of ids.
        """

class AsyncSonyFlake:
    """Async wrapper for :class:`SonyFlake`.

    Implements Awaitable and AsyncIterator protocols.

    Important:
        Main state is stored in SonyFlake. This class has minimum logic on its
        own, the only difference is that instead of doing thread-blocking sleep,
        it is delegated to the provided ``sleep``. Instance of
        class:`sleep_wrapper` is returned in ``__call__``, ``__anext__`` and
        ``__await``.

    Usage:

        .. code-block:: python

            import asyncio

            sf = SonyFlake(0x1337, 0xCAFE, start_time=1749081600)
            asf = AsyncSonyFlake(sf, asyncio.sleep)

            print(await asf)
            print(await asf(5))

            async for id_ in asf:
                print(id_)
                break  # AsyncSonyFlake is an infinite generator
    """

    def __init__(self, sf: SonyFlake, sleep: AsyncSleep) -> None:
        """Initialize AsyncSonyFlake ID generator.

        Args:
            sf: Instance of the :class:`SonyFlake`.
            sleep: Either `asyncio.sleep` or `trio.sleep`.

        Raises:
            ValueError: Invalid values of ``machine_id`` or ``start_time``.
            TypeError: ``machine_id`` or ``start_time`` are not integers.
        """

    def __call__(self, n: int) -> Awaitable[list[int]]:
        """Generate multiple SonyFlake IDs at once.

        Roughly equivalent to `[await asf for _ in range(n)]`, but more
        efficient. This method saves on task/context switches and syscalls for
        getting current time.

        Important:
            The more ids you request, the more other coroutines have to wait
            upon the next id(s).

        Args:
            n: Number of ids to generate. Must be greater than 0.

        Raises:
            ValueError: if n <= 0

        Returns:
            List of ids.
        """

    def __await__(self) -> Generator[None, None, int]:
        """Produce a SonyFlake ID."""

    def __anext__(self) -> Awaitable[int]:
        """Produce a SonyFlake ID."""

    def __aiter__(self) -> Self:
        """Returns ``self``."""

class MachineIDLCG:
    """A simple LCG producing ints suitable to be used as ``machine_id``.

    Intended to be used in examples, tests, or when concurrency is not an issue.
    You generally want only one instance of this class per process.
    """

    def __new__(cls, seed: int, /) -> Self:
        """Make a LCG.

        Args:
            seed: Starting seed.
        """

    def __iter__(self) -> Self:
        """Returns ``self``."""

    def __next__(self) -> int:
        """Produce a Machine ID for :class:`SonyFlake`."""

    def __call__(self) -> int:
        """Same as :meth:`__next__`. For usage as a callback."""
