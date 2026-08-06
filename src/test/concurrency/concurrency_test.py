from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from run_blackbox import run_pytest


BASIC_CASES = [
    "concurrency_read_test",
    "dirty_write_test",
    "dirty_read_test",
    "lost_update_test",
    "unrepeatable_read_test",
    "unrepeatable_read_test_hard",
]

if __name__ == "__main__":
    raise SystemExit(
        run_pytest(
            [f"test_concurrency.py::test_concurrency[{case}]" for case in BASIC_CASES],
            ["concurrency_test"],
        )
    )
