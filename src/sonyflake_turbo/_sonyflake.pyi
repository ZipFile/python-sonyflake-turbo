from typing import Optional

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
    def __init__(self, *machine_id: int, start_time: Optional[int] = None): ...
    def __iter__(self) -> Self: ...
    def __next__(self) -> int: ...

class MachineIDLCG:
    def __init__(self, x: int, /) -> None: ...
    def __iter__(self) -> Self: ...
    def __next__(self) -> int: ...
