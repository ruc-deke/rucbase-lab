from pathlib import Path

import pytest
from wire_assertions import assert_typed_output

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


@pytest.mark.parametrize("case_name", BASIC_CASES + BONUS_CASES)
def test_concurrency(case_name, rmdb_server, binary_path) -> None:
    server = rmdb_server()
    result = server.run_client(
        binary_path("--concurrency-client"),
        CONCURRENCY_DIR / f"{case_name}.sql",
        label=case_name,
    )

    expected = CONCURRENCY_DIR / f"{case_name}_output.typed.jsonl"
    assert_typed_output(result.stdout, expected, ordered_rows=case_name in BONUS_CASES)
