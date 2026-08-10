from __future__ import annotations

from collections import Counter
from pathlib import Path

WIRE_COLUMN_WIDTH = 16


def _canonical_cell(value: str) -> str:
    value = value.strip()
    if len(value) > WIRE_COLUMN_WIDTH:
        value = value[: WIRE_COLUMN_WIDTH - 3] + "..."
    return value


def canonical_wire_lines(text: str) -> list[str]:
    """Reduce Wire pretty tables to the historical, order-insensitive row form."""
    result: list[str] = []
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("Total record(s):"):
            continue
        if line.startswith("+") and set(line) <= {"+", "-"}:
            continue
        if line.startswith("|") and line.endswith("|"):
            cells = [_canonical_cell(cell) for cell in line[1:-1].split("|")]
            result.append("| " + " | ".join(cells) + " |")
        else:
            result.append(line)
    return result


def expected_wire_lines(path: Path) -> list[str]:
    return canonical_wire_lines(path.read_text())


def assert_wire_output(actual_text: str, expected_path: Path) -> None:
    assert Counter(canonical_wire_lines(actual_text)) == Counter(expected_wire_lines(expected_path))
