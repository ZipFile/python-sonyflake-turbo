from typing import overload

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

class SonyFlake:
    def __init__(self, *machine_id: int, start_time: int | None = None):
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

    def __call__(self, n: int, /) -> list[int]:
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

    @overload
    def _raw(self, n: int) -> tuple[list[int], float]: ...
    @overload
    def _raw(self, n: None) -> tuple[int, float]: ...

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
