import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from run_blackbox import run_pytest

BONUS_CASES = [f"phantom_read_test_{index}" for index in range(1, 5)]

if __name__ == "__main__":
    raise SystemExit(
        run_pytest(
            [f"test_concurrency.py::test_concurrency[{case}]" for case in BONUS_CASES],
            ["blackbox_concurrency_client"],
        )
    )
