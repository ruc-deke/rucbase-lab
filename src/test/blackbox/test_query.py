from pathlib import Path

import pytest

from wire_assertions import assert_wire_output


QUERY_DIR = Path(__file__).parents[1] / "query" / "query_sql"
QUERY_CASES = [f"basic_query_test{index}" for index in range(1, 6)]


@pytest.mark.parametrize("case_name", QUERY_CASES)
def test_query(case_name, rmdb_server, binary_path) -> None:
    server = rmdb_server()
    result = server.run_client(
        binary_path("--query-client"),
        QUERY_DIR / f"{case_name}.sql",
        label=case_name,
    )
    assert_wire_output(result.stdout, QUERY_DIR / f"basic_query_answer{case_name[-1]}.txt")


def test_regress_client(rmdb_server, binary_path) -> None:
    server = rmdb_server()
    result = server.run_client(
        binary_path("--regress-client"),
        QUERY_DIR / "basic_query_test1.sql",
        label="regress-client",
    )
    assert_wire_output(result.stdout, QUERY_DIR / "basic_query_answer1.txt")
