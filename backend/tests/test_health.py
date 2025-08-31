from http import HTTPStatus

from fastapi.testclient import TestClient

from app.main import create_app


def test_health():
    client = TestClient(create_app())
    r = client.get("/health")
    assert r.status_code == HTTPStatus.OK
    assert r.json() == {"ok": True}
