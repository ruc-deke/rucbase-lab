import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from run_blackbox import run_pytest

if __name__ == "__main__":
    raise SystemExit(
        run_pytest(
            [
                "test_transaction.py::test_transaction[commit_test]",
                "test_transaction.py::test_transaction[abort_test]",
            ],
            ["blackbox_transaction_client"],
        )
    )
