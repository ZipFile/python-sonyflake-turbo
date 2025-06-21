from typing import List, Optional

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
            The more ids you request, the more other threads has to wait
            upon next :meth:`next` or :meth:`n` call.

        Args:
            n: Number of ids to generate. Must be greater than 0.

        Returns:
            List of ids.
        """

class MachineIDLCG:
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
