from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from run_blackbox import run_pytest


if __name__ == "__main__":
    raise SystemExit(run_pytest("test_query.py::test_query", ["query_test"]))
