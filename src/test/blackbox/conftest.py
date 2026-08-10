from __future__ import annotations

import signal
import socket
import subprocess
import time
from pathlib import Path

import pytest

LOG_NAME_CHARACTERS = frozenset("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-")


def safe_log_name(test_name: str) -> str:
    characters: list[str] = []
    previous_was_separator = False
    for character in test_name:
        if character in LOG_NAME_CHARACTERS:
            characters.append(character)
            previous_was_separator = False
        elif not previous_was_separator:
            characters.append("-")
            previous_was_separator = True
    return "".join(characters).strip("-") or "unnamed-test"


def pytest_addoption(parser: pytest.Parser) -> None:
    group = parser.getgroup("rucbase")
    group.addoption("--rmdb-binary")
    group.addoption("--query-client")
    group.addoption("--transaction-client")
    group.addoption("--regress-client")
    group.addoption("--concurrency-client")
    group.addoption("--log-dir", default="test-logs")


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item: pytest.Item, call: pytest.CallInfo):
    outcome = yield
    report = outcome.get_result()
    if not report.failed:
        return

    for log_path in getattr(item, "rucbase_log_paths", []):
        path = Path(log_path)
        if path.exists():
            report.sections.append((f"saved log: {path}", path.read_text(errors="replace")))


@pytest.fixture
def binary_path(pytestconfig: pytest.Config):
    def resolve(option: str) -> Path:
        value = pytestconfig.getoption(option)
        if not value:
            pytest.fail(f"missing required pytest option: {option}", pytrace=False)
        path = Path(value).resolve()
        if not path.is_file():
            pytest.fail(f"test binary does not exist: {path}", pytrace=False)
        return path

    return resolve


@pytest.fixture
def tcp_port_factory():
    def allocate() -> int:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
            probe.bind(("127.0.0.1", 0))
            return probe.getsockname()[1]

    return allocate


class RucbaseServer:
    def __init__(
        self,
        rmdb_binary: Path,
        work_dir: Path,
        port: int,
        log_dir: Path,
        test_name: str,
        request: pytest.FixtureRequest,
    ) -> None:
        self.rmdb_binary = rmdb_binary
        self.work_dir = work_dir
        self.port = port
        self.log_dir = log_dir
        self.database_name = f"database-{port}"
        self.database_dir = work_dir / self.database_name
        safe_name = safe_log_name(test_name)
        self.log_path = log_dir / f"{safe_name}-{port}-server.log"
        self.request = request
        self.process: subprocess.Popen[str] | None = None
        self._log_file = None

    def start(self) -> "RucbaseServer":
        self.log_dir.mkdir(parents=True, exist_ok=True)
        self._remember_log(self.log_path)
        self._log_file = self.log_path.open("w", buffering=1)
        self.process = subprocess.Popen(
            [
                str(self.rmdb_binary),
                "-p",
                str(self.port),
                self.database_name,
            ],
            cwd=self.work_dir,
            stdout=self._log_file,
            stderr=subprocess.STDOUT,
            text=True,
        )
        self._wait_until_ready()
        return self

    def _wait_until_ready(self, timeout: float = 10.0) -> None:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            assert self.process is not None
            if self.process.poll() is not None:
                pytest.fail(
                    f"rmdb exited before becoming ready; see {self.log_path}",
                    pytrace=False,
                )
            try:
                with socket.create_connection(("127.0.0.1", self.port), timeout=0.2):
                    return
            except OSError:
                time.sleep(0.05)
        pytest.fail(f"rmdb readiness timed out; see {self.log_path}", pytrace=False)

    def run_client(
        self,
        executable: Path,
        *arguments: Path | str,
        label: str,
        timeout: float = 60.0,
    ) -> subprocess.CompletedProcess[str]:
        command = [str(executable), "-p", str(self.port), *(str(argument) for argument in arguments)]
        client_log = self.log_path.with_name(self.log_path.stem.replace("-server", f"-{label}") + ".log")
        self._remember_log(client_log)
        try:
            result = subprocess.run(
                command,
                cwd=self.work_dir,
                text=True,
                capture_output=True,
                timeout=timeout,
                check=False,
            )
        except subprocess.TimeoutExpired as error:
            client_log.write_text(
                f"command: {' '.join(command)}\nTIMEOUT after {timeout}s\n"
                f"stdout:\n{error.stdout or ''}\nstderr:\n{error.stderr or ''}\n"
            )
            pytest.fail(f"{label} timed out; see {client_log}", pytrace=False)
            raise AssertionError("pytest.fail unexpectedly returned") from error

        client_log.write_text(
            f"command: {' '.join(command)}\nreturn code: {result.returncode}\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}\n"
        )
        if result.returncode != 0:
            pytest.fail(f"{label} failed with code {result.returncode}; see {client_log}", pytrace=False)
        return result

    def stop(self) -> None:
        if self.process is not None and self.process.poll() is None:
            self.process.send_signal(signal.SIGINT)
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.terminate()
                try:
                    self.process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    self.process.kill()
                    self.process.wait(timeout=2)
        if self._log_file is not None:
            self._log_file.close()

    def _remember_log(self, path: Path) -> None:
        log_paths = getattr(self.request.node, "rucbase_log_paths", [])
        log_paths.append(path)
        self.request.node.rucbase_log_paths = log_paths


@pytest.fixture
def rmdb_server(
    request: pytest.FixtureRequest,
    pytestconfig: pytest.Config,
    tmp_path: Path,
    tcp_port_factory,
    binary_path,
):
    servers: list[RucbaseServer] = []
    log_dir = Path(pytestconfig.getoption("--log-dir")).resolve()
    rmdb_binary = binary_path("--rmdb-binary")

    def start() -> RucbaseServer:
        server = RucbaseServer(
            rmdb_binary=rmdb_binary,
            work_dir=tmp_path,
            port=tcp_port_factory(),
            log_dir=log_dir,
            test_name=request.node.nodeid,
            request=request,
        )
        servers.append(server)
        return server.start()

    yield start

    for server in reversed(servers):
        server.stop()
