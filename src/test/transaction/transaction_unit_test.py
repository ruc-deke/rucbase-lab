import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from run_blackbox import run_pytest

CASES = {"commit_test", "abort_test", "commit_index_test", "abort_index_test"}

if __name__ == "__main__":
    if len(sys.argv) != 2 or sys.argv[1] not in CASES:
        raise SystemExit("Usage: python3 transaction_unit_test.py <test_case_name>")
    raise SystemExit(
        run_pytest(f"test_transaction.py::test_transaction[{sys.argv[1]}]", ["blackbox_transaction_client"])
    )
