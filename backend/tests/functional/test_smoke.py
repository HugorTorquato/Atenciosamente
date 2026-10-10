# The thinnest possible proof that the suite can reach a running server. It is
# indifferent to the database, but the autouse clean_database fixture still
# applies, so it needs a reachable, migrated Postgres to run at all.
def test_root_answers_hello(api):
    res = api.get("/")
    assert res.status_code == 200
    assert res.text == "hello"
