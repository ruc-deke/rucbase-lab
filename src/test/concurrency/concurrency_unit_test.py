import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from run_blackbox import run_pytest

CASES = {
    "concurrency_read_test",
    "dirty_write_test",
    "dirty_read_test",
    "lost_update_test",
    "unrepeatable_read_test",
    "unrepeatable_read_test_hard",
    "phantom_read_test_1",
    "phantom_read_test_2",
    "phantom_read_test_3",
    "phantom_read_test_4",
}

if __name__ == "__main__":
    if len(sys.argv) != 2 or sys.argv[1] not in CASES:
        raise SystemExit("Usage: python3 concurrency_unit_test.py <test_case_name>")
    raise SystemExit(
        run_pytest(f"test_concurrency.py::test_concurrency[{sys.argv[1]}]", ["blackbox_concurrency_client"])
    )
