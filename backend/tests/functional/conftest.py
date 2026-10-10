import os

import psycopg
import pytest
import requests

DEFAULT_BASE_URL = "http://localhost:8080"


class ApiClient:
    # requests applies no default timeout, and there is no CTest backstop here:
    # without these a hung server hangs the whole suite indefinitely.
    TIMEOUT = (2, 5)  # (connect, read) seconds

    def __init__(self, base_url):
        self._base_url = base_url
        self._session = requests.Session()

    def get(self, path):
        return self._session.get(self._base_url + path, timeout=self.TIMEOUT)

    # body is a raw str, never a dict: several cases must send bodies json.dumps
    # could not produce ("not json", a bare "[]").
    def post(self, path, body):
        return self._session.post(
            self._base_url + path,
            data=body.encode("utf-8"),
            headers={"Content-Type": "application/json"},
            timeout=self.TIMEOUT,
        )

    # For the wrong-method cases: no body, just a method and a path.
    def request(self, method, path):
        return self._session.request(method, self._base_url + path, timeout=self.TIMEOUT)


@pytest.fixture(scope="session")
def base_url():
    return os.environ.get("ATENCIOSAMENTE_BASE_URL", DEFAULT_BASE_URL)


@pytest.fixture(scope="session")
def db_url():
    # os.environ[...] rather than .get(): a missing variable raises KeyError and
    # fails the run loudly, matching read_env() in src/db/connection.cpp.
    #
    # TODO(data-management): this suite TRUNCATEs the database named by POSTGRES_DB,
    # which today is the dev database, deliberately — every test starts from an empty
    # table and none may rely on rows an earlier test or a manual curl left behind.
    # That is only safe while the single database is local and disposable. Before
    # Phase 5 points this at anything non-local, reinstate a name guard here.
    return (
        f"postgresql://{os.environ['POSTGRES_USER']}:{os.environ['POSTGRES_PASSWORD']}"
        f"@{os.environ['POSTGRES_HOST']}:{os.environ['POSTGRES_PORT']}"
        f"/{os.environ['POSTGRES_DB']}"
    )


# autouse: every test starts from an empty table without asking. Reset-before with
# no teardown, so a failed test leaves its rows behind for you to inspect.
# RESTART IDENTITY so the id sequence restarts too and ids are stable across runs.
@pytest.fixture(autouse=True)
def clean_database(db_url):
    with psycopg.connect(db_url, autocommit=True) as conn:
        conn.execute("TRUNCATE notifications RESTART IDENTITY")


@pytest.fixture
def api(base_url):
    return ApiClient(base_url)


# S4's concurrency test needs one client per thread, so it asks for the factory
# rather than importing ApiClient out of conftest.
@pytest.fixture
def make_api(base_url):
    return lambda: ApiClient(base_url)
