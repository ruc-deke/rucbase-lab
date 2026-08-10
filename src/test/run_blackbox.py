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
    # Always perform an incremental build: file existence alone cannot tell
    # whether a binary reflects the current source tree.
    subprocess.run(
        ["cmake", "-S", str(REPOSITORY_ROOT), "-B", str(build_dir), "-DCMAKE_BUILD_TYPE=Debug"],
        cwd=REPOSITORY_ROOT,
        check=True,
    )
    subprocess.run(
        ["cmake", "--build", str(build_dir), "--target", *required_targets, "-j", "4"],
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
        str(binary_dir / "blackbox_query_client"),
        "--transaction-client",
        str(binary_dir / "blackbox_transaction_client"),
        "--regress-client",
        str(binary_dir / "blackbox_regress_client"),
        "--concurrency-client",
        str(binary_dir / "blackbox_concurrency_client"),
        "--log-dir",
        str(build_dir / "test-logs"),
    ]
    return subprocess.run(command, cwd=REPOSITORY_ROOT, check=False).returncode
