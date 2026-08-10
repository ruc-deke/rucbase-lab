from collections import Counter
from pathlib import Path

import pytest

CONCURRENCY_DIR = Path(__file__).parents[1] / "concurrency" / "concurrency_sql"
BASIC_CASES = [
    "concurrency_read_test",
    "dirty_write_test",
    "dirty_read_test",
    "lost_update_test",
    "unrepeatable_read_test",
    "unrepeatable_read_test_hard",
]
BONUS_CASES = [f"phantom_read_test_{index}" for index in range(1, 5)]


def nonempty_lines(path: Path) -> list[str]:
    assert path.is_file(), f"expected output file was not created: {path}"
    return [line.rstrip("\n") for line in path.read_text().splitlines() if line]


def normalized_lines(path: Path) -> list[str]:
    return ["".join(line.split()) for line in nonempty_lines(path)]


@pytest.mark.parametrize("case_name", BASIC_CASES + BONUS_CASES)
def test_concurrency(case_name, rmdb_server, binary_path) -> None:
    server = rmdb_server()
    result = server.run_client(
        binary_path("--concurrency-client"),
        CONCURRENCY_DIR / f"{case_name}.sql",
        label=case_name,
    )

    expected = CONCURRENCY_DIR / f"{case_name}_output.txt"
    if case_name in BASIC_CASES:
        assert Counter(result.stdout.splitlines()) == Counter(nonempty_lines(expected))
    else:
        actual = ["".join(line.split()) for line in result.stdout.splitlines() if line]
        assert actual == normalized_lines(expected)
