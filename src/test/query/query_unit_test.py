import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from run_blackbox import run_pytest

QUERY_CASES = {f"basic_query_test{index}.sql": f"basic_query_test{index}" for index in range(1, 6)}


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: python3 query_unit_test.py basic_query_test<N>.sql")
    case_name = QUERY_CASES.get(Path(sys.argv[1]).name)
    if case_name is None:
        raise SystemExit(f"Unknown query test: {sys.argv[1]}")
    raise SystemExit(run_pytest(f"test_query.py::test_query[{case_name}]", ["blackbox_query_client"]))
