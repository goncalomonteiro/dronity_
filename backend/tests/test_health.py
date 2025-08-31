from http import HTTPStatus
from app.main import create_app
from fastapi.testclient import TestClient


def test_health():
    client = TestClient(create_app())
    r = client.get("/health")
    assert r.status_code == HTTPStatus.OK
    assert r.json() == {"ok": True}
