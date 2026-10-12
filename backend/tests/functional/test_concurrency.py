# Phase 2's pool tests, proved from outside: more concurrent requests than the
# server has pooled connections, every one landing exactly once.
import json
from concurrent.futures import ThreadPoolExecutor

THREADS, PER_THREAD = 8, 10
TOTAL = THREADS * PER_THREAD


def _post_batch(thread_index, make_api):
    # Its own client: ApiClient owns a requests.Session, which is not thread-safe.
    api = make_api()
    results = []
    for n in range(PER_THREAD):
        title = f"t{thread_index}-n{n}"
        res = api.post(
            "/notifications", json.dumps({"title": title, "body": "concurrent"})
        )
        results.append((title, res.status_code, res.json().get("id")))
    return results


# requests releases the GIL on blocking socket calls, so all 8 requests really are
# in flight at once; the contention proven is in the server's pool, not Python's.
# How many reach a handler is Crow's call — .multithreaded() gives it
# hardware_concurrency() threads (4 here), still above POSTGRES_POOL_SIZE=2, but on
# a single-core host this would serialize and stop exercising the pool.
#
# ApiClient's 5s read timeout is the only backstop in this tier — no CTest timeout
# exists here — so a pool deadlock fails this test instead of hanging the suite.
def test_parallel_posts_all_land_exactly_once(api, make_api):
    # Per-thread return values, merged here: no shared mutable state, so no lock.
    with ThreadPoolExecutor(max_workers=THREADS) as pool:
        batches = list(pool.map(lambda i: _post_batch(i, make_api), range(THREADS)))

    results = [result for batch in batches for result in batch]
    assert len(results) == TOTAL

    # Listed, not counted, so a failure names the titles that didn't get a 201.
    assert [(title, status) for title, status, _ in results if status != 201] == []

    # Duplicate ids would mean one connection served two INSERT ... RETURNINGs.
    ids = [notification_id for _, _, notification_id in results]
    assert len(set(ids)) == TOTAL

    # The GET proves the rows committed, not just that the server said so. Sets,
    # never lists: the order 80 concurrent INSERTs land in is not part of the
    # contract, and every assertion here is on an outcome, never on timing.
    sent_titles = {title for title, _, _ in results}
    listed = api.get("/notifications").json()
    assert len(listed) == TOTAL
    assert {notification["title"] for notification in listed} == sent_titles
