from __future__ import annotations

import json
import math
import struct
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Any

SQL_TYPES = frozenset({"int32", "float32", "string"})
RESULT_STATUS = "result_set"
ERROR_STATUSES = frozenset({"sql_error", "transaction_abort"})


@dataclass(frozen=True)
class TypedEvent:
    status: str
    columns: tuple[tuple[str, str], ...] = ()
    rows: tuple[tuple[Any, ...], ...] = ()


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def _cell_value(sql_type: str, value: Any, location: str) -> Any:
    if value is None:
        return None
    if sql_type == "int32":
        _require(
            isinstance(value, int)
            and not isinstance(value, bool)
            and -(2**31) <= value < 2**31,
            f"{location}: invalid INT32 value",
        )
        return value
    if sql_type == "float32":
        _require(
            isinstance(value, (int, float))
            and not isinstance(value, bool)
            and math.isfinite(float(value)),
            f"{location}: invalid FLOAT32 value",
        )
        try:
            return struct.unpack("!I", struct.pack("!f", value))[0]
        except (OverflowError, struct.error) as error:
            raise ValueError(f"{location}: FLOAT32 value is out of range") from error
    _require(isinstance(value, str), f"{location}: STRING value must be JSON text")
    return value


def parse_typed_jsonl(text: str) -> list[TypedEvent]:
    """Parse typed result events emitted by the black-box clients."""
    events: list[TypedEvent] = []
    for line_number, raw_line in enumerate(text.split("\n"), start=1):
        if not raw_line.strip():
            continue
        location = f"JSONL line {line_number}"
        try:
            item = json.loads(raw_line)
        except json.JSONDecodeError as error:
            raise ValueError(f"{location}: invalid JSON: {error.msg}") from error

        _require(isinstance(item, dict), f"{location}: event must be an object")
        status = item.get("status")
        _require(
            status == RESULT_STATUS or status in ERROR_STATUSES,
            f"{location}: invalid event status {status!r}",
        )
        if status in ERROR_STATUSES:
            _require(
                set(item) == {"status"},
                f"{location}: error event has unexpected fields",
            )
            events.append(TypedEvent(status))
            continue

        _require(
            set(item) == {"status", "columns", "rows"},
            f"{location}: result event has missing or unexpected fields",
        )
        raw_columns = item["columns"]
        _require(
            isinstance(raw_columns, list) and raw_columns,
            f"{location}: columns must be non-empty",
        )
        columns: list[tuple[str, str]] = []
        for column_index, raw_column in enumerate(raw_columns):
            column_location = f"{location}, column {column_index}"
            _require(
                isinstance(raw_column, dict) and set(raw_column) == {"name", "type"},
                f"{column_location}: invalid column definition",
            )
            name, sql_type = raw_column["name"], raw_column["type"]
            _require(
                isinstance(name, str) and name,
                f"{column_location}: invalid column name",
            )
            _require(
                sql_type in SQL_TYPES,
                f"{column_location}: unknown SQL type {sql_type!r}",
            )
            columns.append((name, sql_type))

        raw_rows = item["rows"]
        _require(isinstance(raw_rows, list), f"{location}: rows must be an array")
        rows: list[tuple[Any, ...]] = []
        for row_index, raw_row in enumerate(raw_rows):
            row_location = f"{location}, row {row_index}"
            _require(
                isinstance(raw_row, list) and len(raw_row) == len(columns),
                f"{row_location}: row does not match schema",
            )
            rows.append(
                tuple(
                    _cell_value(
                        column_type, value, f"{row_location}, cell {column_index}"
                    )
                    for column_index, ((_, column_type), value) in enumerate(
                        zip(columns, raw_row)
                    )
                )
            )
        events.append(TypedEvent(RESULT_STATUS, tuple(columns), tuple(rows)))
    return events


def assert_typed_output(
    actual_text: str, expected_path: Path, *, ordered_rows: bool = False
) -> None:
    _require(
        expected_path.name.endswith(".typed.jsonl"),
        f"expected fixture must be an explicit .typed.jsonl file: {expected_path}",
    )
    actual = parse_typed_jsonl(actual_text)
    expected = parse_typed_jsonl(expected_path.read_text(encoding="utf-8"))
    assert len(actual) == len(expected), (
        f"event count differs: actual {len(actual)}, expected {len(expected)}"
    )

    for event_index, (actual_event, expected_event) in enumerate(zip(actual, expected)):
        assert actual_event.status == expected_event.status, (
            f"event {event_index} status differs"
        )
        if actual_event.status != RESULT_STATUS:
            continue
        assert actual_event.columns == expected_event.columns, (
            f"event {event_index} schema differs"
        )
        if ordered_rows:
            assert actual_event.rows == expected_event.rows, (
                f"event {event_index} rows differ"
            )
        else:
            actual_rows, expected_rows = (
                Counter(actual_event.rows),
                Counter(expected_event.rows),
            )
            assert actual_rows == expected_rows, (
                f"event {event_index} row multiset differs; "
                f"missing={expected_rows - actual_rows!r}, unexpected={actual_rows - expected_rows!r}"
            )
