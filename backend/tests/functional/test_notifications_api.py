# The /notifications contract, asserted from outside the server process: real
# socket, real Crow router, real Postgres. Nothing in here links against the
# backend, so it can only ever see what the phone sees.
#
# One test function per behaviour, so a failure names the part of the contract
# that broke. The autouse clean_database fixture truncates before every test, so
# no test may rely on rows another test wrote — if it needs data, it POSTs it.
import json
import re

import pytest

JSON_CONTENT_TYPE = "application/json"

# src/domain/notification_json.cpp formats timestamps with "{:%FT%T}Z" after
# flooring to seconds: no sub-second part, no numeric offset, literal trailing Z.
CREATED_AT_PATTERN = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")


def post_notification(api, title, body):
    # ApiClient.post sends a raw str, so the encoding choice is ours:
    # ensure_ascii=False puts real UTF-8 bytes on the wire rather than \uXXXX
    # escapes, which is what the UTF-8 test below needs to be meaningful.
    return api.post(
        "/notifications",
        json.dumps({"title": title, "body": body}, ensure_ascii=False),
    )


def test_get_notifications_on_empty_database_returns_empty_array(api):
    res = api.get("/notifications")

    assert res.status_code == 200
    assert res.headers["Content-Type"] == JSON_CONTENT_TYPE
    # Also the proof that clean_database actually ran: without it this is the
    # one assertion in the suite that every later test silently depends on.
    assert res.json() == []


def test_post_valid_notification_returns_201_with_server_generated_fields(api):
    title = "Dentist"
    body = "Tuesday at 10h"

    res = post_notification(api, title, body)

    assert res.status_code == 201
    assert res.headers["Content-Type"] == JSON_CONTENT_TYPE

    created = res.json()
    # id and created_at come from Postgres (SERIAL and DEFAULT now()), so this
    # is the only tier that can assert the database really assigned them.
    assert created["id"] > 0
    assert CREATED_AT_PATTERN.match(created["created_at"])
    assert created["title"] == title
    assert created["body"] == body


def test_posted_notification_comes_back_from_get(api):
    created = post_notification(api, "Round trip", "there and back").json()

    listed = api.get("/notifications").json()

    # Equality on the whole object, not field-by-field: the INSERT ... RETURNING
    # row and the later SELECT row must agree, including the timestamp
    # formatting, or the app would see a notification change shape on refresh.
    assert created in listed
    assert len(listed) == 1


def test_get_returns_newest_notification_first(api):
    first_title = "A"
    second_title = "B"

    first = post_notification(api, first_title, "older").json()
    second = post_notification(api, second_title, "newer").json()

    listed = api.get("/notifications").json()

    # ORDER BY created_at DESC, id DESC. Both rows land inside the same second
    # here, so it is the id tie-break doing the work — which is exactly why the
    # tie-break is in the query: without it this ordering would be arbitrary.
    assert [n["id"] for n in listed] == [second["id"], first["id"]]
    assert [n["title"] for n in listed] == [second_title, first_title]


# The messages are owned by src/domain/create_notification_request.cpp (plus the
# parse branch in src/handlers/notifications.cpp). The unit tests prove that
# function *returns* them; these prove they *reach the wire*, with a 400.
INVALID_BODIES = [
    ("not json", "request body must be valid JSON"),
    ("[]", "request body must be a JSON object"),
    (json.dumps({"title": "x"}), "title and body are required"),
    (json.dumps({"title": 1, "body": "x"}), "title and body must be strings"),
    (json.dumps({"title": "", "body": "x"}), "title and body must not be empty"),
]


@pytest.mark.parametrize("body,expected_error", INVALID_BODIES)
def test_invalid_post_is_rejected_and_writes_nothing(api, body, expected_error):
    res = api.post("/notifications", body)

    assert res.status_code == 400
    # res.json()["error"], never the raw text: key order and whitespace in the
    # serialized body are not part of the contract.
    assert res.json()["error"] == expected_error

    # The reason this assertion belongs here and nowhere else: a unit test can
    # prove the validator returned an error, but only an outside observer can
    # prove no row was written on the way to that error.
    assert api.get("/notifications").json() == []


def test_utf8_text_survives_the_round_trip(api):
    title = "Consulta às 10h — não esqueça"
    body = "Trazer exames anteriores · cartão do convênio"

    created = post_notification(api, title, body).json()
    assert created["title"] == title
    assert created["body"] == body

    # The POST response is built in-process from the INSERT ... RETURNING row,
    # so it could pass while the stored bytes were mangled. The GET is the half
    # that proves the text survived Crow → libpqxx → Postgres → nlohmann → wire.
    listed = api.get("/notifications").json()
    assert listed[0]["title"] == title
    assert listed[0]["body"] == body


def test_unknown_path_returns_404(api):
    res = api.get("/does-not-exist")

    assert res.status_code == 404


def test_wrong_method_on_notifications_returns_405(api):
    res = api.request("PUT", "/notifications")

    # Verified empirically against Crow/master: a path that has *some* route
    # registered but not for this method gets a real 405, not the 404 the
    # router's fallthrough gives an unknown path. Crow answers it itself,
    # before any handler runs, so the body is its plain-text "405 Method Not
    # Allowed" — not our JSON {"error": ...} shape, and with no Allow header.
    assert res.status_code == 405
