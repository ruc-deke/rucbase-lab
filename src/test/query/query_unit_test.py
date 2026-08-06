from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from run_blackbox import run_pytest


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: python3 query_unit_test.py basic_query_test<N>.sql")
    match = re.search(r"basic_query_test([1-5])", sys.argv[1])
    if not match:
        raise SystemExit(f"Unknown query test: {sys.argv[1]}")
    case_name = f"basic_query_test{match.group(1)}"
    raise SystemExit(
        run_pytest(f"test_query.py::test_query[{case_name}]", ["query_test"])
    )
