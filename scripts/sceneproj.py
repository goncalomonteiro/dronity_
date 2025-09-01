#!/usr/bin/env python3
"""
Scene Project (.sceneproj) package utility

Creates and manages desktop project packages:
- Package structure: <name>.sceneproj/ with project.db (SQLite WAL), /assets, /bakes, /thumbs, /snapshots
- Applies migrations under desktop/schema/migrations
- Provides commands to create/open, add scene/track/keyframe, autosave, and restore

Usage:
  python scripts/sceneproj.py [--json] create <path.sceneproj> --name "My Project"
  python scripts/sceneproj.py [--json] open <path.sceneproj>
  python scripts/sceneproj.py [--json] add-scene <path.sceneproj> --name "Scene A" [--id <uuid>]
  python scripts/sceneproj.py [--json] add-track <path.sceneproj> --scene-id <uuid> --name Track1 --kind curve [--id <uuid>]
  python scripts/sceneproj.py [--json] add-key <path.sceneproj> --track-id <uuid> --t 1000 --value '{"x":1}' --interp auto [--id <uuid>]
  python scripts/sceneproj.py [--json] autosave <path.sceneproj> --slot 1
  python scripts/sceneproj.py [--json] restore <path.sceneproj> --slot 1
"""
from __future__ import annotations

import argparse
import json
import shutil
import sqlite3
import sys
import time
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MIGRATIONS_DIR = ROOT / "desktop" / "schema" / "migrations"


def ensure_package(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)
    for sub in ("assets", "bakes", "thumbs", "snapshots"):
        (path / sub).mkdir(exist_ok=True)


def connect_db(db_path: Path) -> sqlite3.Connection:
    conn = sqlite3.connect(db_path)
    conn.execute("PRAGMA foreign_keys=ON;")
    conn.execute("PRAGMA journal_mode=WAL;")
    conn.execute("PRAGMA synchronous=NORMAL;")
    return conn


def apply_migrations(conn: sqlite3.Connection) -> None:
    conn.execute(
        "CREATE TABLE IF NOT EXISTS schema_migrations(version INTEGER PRIMARY KEY, applied_at INTEGER NOT NULL)"
    )
    cur = conn.execute("SELECT COALESCE(MAX(version), 0) FROM schema_migrations")
    current = cur.fetchone()[0] or 0
    migrations = sorted(p for p in MIGRATIONS_DIR.glob("V*.sql"))
    for mig in migrations:
        version = int(mig.name.split("__")[0][1:])
        if version <= current:
            continue
        with open(mig, "r", encoding="utf-8") as f:
            sql = f.read()
        with conn:
            conn.executescript(sql)


def create(path: Path, name: str, json_out: bool) -> None:
    ensure_package(path)
    db_path = path / "project.db"
    conn = connect_db(db_path)
    try:
        apply_migrations(conn)
        pid = str(uuid.uuid4())
        now = int(time.time())
        with conn:
            conn.execute(
                "INSERT INTO projects(id, name, version, created_at, updated_at) VALUES(?,?,?,?,?)",
                (pid, name, 1, now, now),
            )
        if json_out:
            print(json.dumps({"ok": True, "project": {"id": pid, "name": name}, "path": str(path)}))
        else:
            print(f"Created project {name} ({pid}) at {path}")
    finally:
        conn.close()


def open_pkg(path: Path, json_out: bool) -> None:
    db_path = path / "project.db"
    conn = connect_db(db_path)
    try:
        apply_migrations(conn)
        proj = conn.execute("SELECT id, name, version FROM projects LIMIT 1").fetchone()
        scenes = conn.execute("SELECT id, name FROM scenes").fetchall()
        payload = {
            "project": {"id": proj[0], "name": proj[1], "version": proj[2]},
            "scenes": [{"id": s[0], "name": s[1]} for s in scenes],
        }
        if json_out:
            print(json.dumps({"ok": True, **payload}))
        else:
            print(json.dumps(payload, indent=2))
    finally:
        conn.close()


def add_scene(path: Path, name: str, forced_id: str | None, json_out: bool) -> None:
    conn = connect_db(path / "project.db")
    try:
        proj = conn.execute("SELECT id FROM projects LIMIT 1").fetchone()
        if not proj:
            raise RuntimeError("No project found")
        sid = forced_id or str(uuid.uuid4())
        now = int(time.time())
        with conn:
            conn.execute(
                "INSERT INTO scenes(id, project_id, name, created_at, updated_at) VALUES(?,?,?,?,?)",
                (sid, proj[0], name, now, now),
            )
        if json_out:
            print(
                json.dumps(
                    {
                        "ok": True,
                        "scene": {"id": sid, "name": name},
                    }
                )
            )
        else:
            print(f"Added scene {name} ({sid})")
    finally:
        conn.close()


def add_track(
    path: Path,
    scene_id: str,
    name: str,
    kind: str,
    forced_id: str | None,
    json_out: bool,
) -> None:
    conn = connect_db(path / "project.db")
    try:
        tid = forced_id or str(uuid.uuid4())
        now = int(time.time())
        with conn:
            conn.execute(
                "INSERT INTO tracks(id, scene_id, name, kind, created_at, updated_at) VALUES(?,?,?,?,?,?)",
                (tid, scene_id, name, kind, now, now),
            )
        if json_out:
            print(
                json.dumps(
                    {
                        "ok": True,
                        "track": {
                            "id": tid,
                            "scene_id": scene_id,
                            "name": name,
                            "kind": kind,
                        },
                    }
                )
            )
        else:
            print(f"Added track {name} ({tid}) to scene {scene_id}")
    finally:
        conn.close()


def add_key(
    path: Path,
    track_id: str,
    t_ms: int,
    value_json: str,
    interp: str,
    forced_id: str | None,
    json_out: bool,
) -> None:
    conn = connect_db(path / "project.db")
    try:
        # validate JSON early
        json.loads(value_json)
        kid = forced_id or str(uuid.uuid4())
        now = int(time.time())
        with conn:
            conn.execute(
                "INSERT INTO keyframes(id, track_id, t_ms, value_json, interp, created_at, updated_at) VALUES(?,?,?,?,?,?,?)",
                (kid, track_id, t_ms, value_json, interp, now, now),
            )
        if json_out:
            print(
                json.dumps(
                    {
                        "ok": True,
                        "keyframe": {
                            "id": kid,
                            "track_id": track_id,
                            "t_ms": t_ms,
                        },
                    }
                )
            )
        else:
            print(f"Added keyframe ({kid}) at {t_ms}ms to track {track_id}")
    finally:
        conn.close()


def autosave(path: Path, slot: int, json_out: bool) -> None:
    src = path / "project.db"
    dst = path / "snapshots" / f"slot{slot}.db"
    # Ensure WAL checkpoint before copy
    conn = connect_db(src)
    try:
        conn.execute("PRAGMA wal_checkpoint(FULL);")
    finally:
        conn.close()
    shutil.copy2(src, dst)
    if json_out:
        print(json.dumps({"ok": True, "snapshot": str(dst)}))
    else:
        print(f"Autosaved to {dst}")


def restore(path: Path, slot: int, json_out: bool) -> None:
    src = path / "snapshots" / f"slot{slot}.db"
    dst = path / "project.db"
    if not src.exists():
        raise FileNotFoundError(src)
    shutil.copy2(src, dst)
    if json_out:
        print(json.dumps({"ok": True, "restored_from": str(src)}))
    else:
        print(f"Restored project.db from {src}")


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    ap.add_argument("--json", action="store_true", help="Emit JSON output for scripting")

    ap_create = sub.add_parser("create")
    ap_create.add_argument("path")
    ap_create.add_argument("--name", required=True)

    ap_open = sub.add_parser("open")
    ap_open.add_argument("path")

    ap_add_scene = sub.add_parser("add-scene")
    ap_add_scene.add_argument("path")
    ap_add_scene.add_argument("--name", required=True)
    ap_add_scene.add_argument("--id", dest="forced_id")

    ap_add_track = sub.add_parser("add-track")
    ap_add_track.add_argument("path")
    ap_add_track.add_argument("--scene-id", required=True)
    ap_add_track.add_argument("--name", required=True)
    ap_add_track.add_argument("--kind", default="curve")
    ap_add_track.add_argument("--id", dest="forced_id")

    ap_add_key = sub.add_parser("add-key")
    ap_add_key.add_argument("path")
    ap_add_key.add_argument("--track-id", required=True)
    ap_add_key.add_argument("--t", type=int, required=True)
    ap_add_key.add_argument("--value", required=True)
    ap_add_key.add_argument("--interp", default="auto")
    ap_add_key.add_argument("--id", dest="forced_id")

    ap_autosave = sub.add_parser("autosave")
    ap_autosave.add_argument("path")
    ap_autosave.add_argument("--slot", type=int, default=1)

    ap_restore = sub.add_parser("restore")
    ap_restore.add_argument("path")
    ap_restore.add_argument("--slot", type=int, default=1)

    args = ap.parse_args(argv)
    pkg = Path(args.path)
    if pkg.suffix != ".sceneproj":
        pkg = pkg.with_suffix(".sceneproj")

    try:
        if args.cmd == "create":
            create(pkg, args.name, args.json)
        elif args.cmd == "open":
            open_pkg(pkg, args.json)
        elif args.cmd == "add-scene":
            add_scene(pkg, args.name, args.forced_id, args.json)
        elif args.cmd == "add-track":
            add_track(pkg, args.scene_id, args.name, args.kind, args.forced_id, args.json)
        elif args.cmd == "add-key":
            add_key(pkg, args.track_id, args.t, args.value, args.interp, args.forced_id, args.json)
        elif args.cmd == "autosave":
            autosave(pkg, args.slot, args.json)
        elif args.cmd == "restore":
            restore(pkg, args.slot, args.json)
    except Exception as e:  # noqa: BLE001
        if args.json:
            print(json.dumps({"ok": False, "error": str(e)}))
        else:
            print(f"Error: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
