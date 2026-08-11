from pathlib import Path

import pytest
from wire_assertions import assert_typed_output

TRANSACTION_DIR = Path(__file__).parents[1] / "transaction" / "transaction_sql"
TRANSACTION_CASES = [
    "commit_test",
    "abort_test",
    "commit_index_test",
    "abort_index_test",
]


@pytest.mark.parametrize("case_name", TRANSACTION_CASES)
def test_transaction(case_name, rmdb_server, binary_path) -> None:
    server = rmdb_server()
    result = server.run_client(
        binary_path("--transaction-client"),
        TRANSACTION_DIR / f"{case_name}.sql",
        label=case_name,
    )
    assert_typed_output(
        result.stdout, TRANSACTION_DIR / f"{case_name}_output.typed.jsonl"
    )
