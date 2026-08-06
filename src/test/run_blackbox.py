"""Compatibility entry point for the lab handouts' historical Python commands."""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
BLACKBOX_DIR = Path(__file__).resolve().parent / "blackbox"


def run_pytest(test_nodes: str | list[str], targets: list[str]) -> int:
    build_dir = Path(os.environ.get("RUCBASE_BUILD_DIR", REPOSITORY_ROOT / "build" / "debug"))
    binary_dir = build_dir / "bin"
    required_targets = ["rmdb", *targets]
    if any(not (binary_dir / target).is_file() for target in required_targets):
        subprocess.run(["cmake", "--preset", "debug"], cwd=REPOSITORY_ROOT, check=True)
        subprocess.run(
            ["cmake", "--build", "--preset", "debug", "--target", *required_targets, "-j", "4"],
            cwd=REPOSITORY_ROOT,
            check=True,
        )

    if isinstance(test_nodes, str):
        test_nodes = [test_nodes]

    command = [
        sys.executable,
        "-m",
        "pytest",
        "-q",
        *(str(BLACKBOX_DIR / test_node) for test_node in test_nodes),
        "--rmdb-binary",
        str(binary_dir / "rmdb"),
        "--query-client",
        str(binary_dir / "query_test"),
        "--transaction-client",
        str(binary_dir / "transaction_test"),
        "--regress-client",
        str(binary_dir / "regress_test"),
        "--concurrency-client",
        str(binary_dir / "concurrency_test"),
        "--log-dir",
        str(build_dir / "test-logs"),
    ]
    return subprocess.run(command, cwd=REPOSITORY_ROOT, check=False).returncode
