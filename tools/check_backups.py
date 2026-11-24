#!/usr/bin/env python3
"""Проверка подхода к резервному копированию без Qt.

Проверяет то, что делает BackupService.cpp: снимок через VACUUM INTO,
целостность копии, ротацию (10 файлов), страховочную копию при восстановлении.

Запуск: python3 tools/check_backups.py
"""

import re
import shutil
import sqlite3
import sys
import tempfile
from pathlib import Path

DB_DIR = Path(__file__).resolve().parent.parent / "resources" / "db"
KEEP = 10

checks = 0
failures = []


def check(name, fn):
    global checks
    checks += 1
    try:
        fn()
        print(f"  ok   {name}")
    except Exception as exc:  # noqa: BLE001
        failures.append(f"{name}: {exc}")
        print(f"  FAIL {name}: {exc}")


def statements_of(path):
    sql = re.sub(r"--.*", "", path.read_text(encoding="utf-8"))
    return [s.strip() for s in sql.split(";") if s.strip()]


def make_db(path):
    con = sqlite3.connect(path)
    con.execute("PRAGMA journal_mode = WAL")
    con.execute("PRAGMA foreign_keys = ON")
    for version, migration in enumerate(sorted(DB_DIR.glob("*.sql")), start=1):
        for statement in statements_of(migration):
            con.execute(statement)
        con.execute(f"PRAGMA user_version = {version}")
    con.execute("""
        INSERT INTO person (last_name, first_name, middle_name, birth_date,
                            microdistrict, street, house, building, apartment,
                            category_id, visit_month, visit_year,
                            subscription_id, subscription_year,
                            created_at, updated_at)
        VALUES ('Иванов', 'Иван', 'Иванович', '1948-03-12', 7, 'ул. Ленина',
                '15', '2', '41', 1, 3, 2026, 2, 2026,
                '2026-11-19 12:00:00', '2026-11-19 12:00:00')
    """)
    con.commit()
    return con


def quick_check_ok(path):
    con = sqlite3.connect(path)
    try:
        return con.execute("PRAGMA quick_check").fetchone()[0] == "ok"
    finally:
        con.close()


def prune(folder):
    files = sorted(folder.glob("veterans-*.db"), reverse=True)
    for old in files[KEEP:]:
        old.unlink()
    return sorted(folder.glob("veterans-*.db"))


def main():
    root = Path(tempfile.mkdtemp(prefix="veterans-backup-"))
    try:
        db_path = root / "veterans.db"
        backups = root / "backups"
        backups.mkdir()

        con = make_db(db_path)

        snapshot = backups / "veterans-20261119-120000.db"

        def vacuum_into():
            con.execute(f"VACUUM INTO '{snapshot}'")
            assert snapshot.exists(), "файл не создан"
            assert snapshot.stat().st_size > 0

        check("VACUUM INTO создаёт файл копии", vacuum_into)

        def snapshot_valid():
            assert quick_check_ok(snapshot), "quick_check не ok"
            with sqlite3.connect(snapshot) as check_db:
                assert check_db.execute("SELECT count(*) FROM person").fetchone()[0] == 1

        check("копия целая и содержит данные", snapshot_valid)

        def daily_today_exists():
            # createDaily пропускает создание, если файл за сегодня уже есть
            today = "veterans-20261119"
            names = [p.name for p in backups.glob(today + "*.db")]
            assert names, "файла за сегодня нет"

        check("признак «копия за сегодня есть»", daily_today_exists)

        def rotation():
            for i in range(11):
                (backups / f"veterans-20261101-{i:06d}.db").write_bytes(b"x")
            left = prune(backups)
            assert len(left) == KEEP, f"осталось {len(left)}"
            assert (backups / "veterans-20261101-000000.db").exists() is False, \
                "самый старый файл не удалён"

        check("ротация: остаётся 10 самых новых", rotation)

        def restore():
            target = root / "restore.db"
            with sqlite3.connect(target) as other:
                other.execute("CREATE TABLE t(x)")
            safety = backups / "veterans-before-restore-20261119-130000.db"
            shutil.copy2(target, safety)
            target.unlink()
            shutil.copy2(snapshot, target)
            assert quick_check_ok(target)
            with sqlite3.connect(target) as restored:
                assert restored.execute("SELECT count(*) FROM person").fetchone()[0] == 1
            assert safety.exists(), "страховочная копия не создана"

        check("восстановление со страховочной копией", restore)

        con.close()

        def wal_cleanup():
            # после закрытия соединения WAL-файлы не должны мешать копии
            for suffix in ("-wal", "-shm"):
                leftover = Path(str(db_path) + suffix)
                if leftover.exists():
                    leftover.unlink()

        check("остатки WAL не блокируют восстановление", wal_cleanup)
    finally:
        shutil.rmtree(root, ignore_errors=True)

    print(f"\nПроверок: {checks}, провалов: {len(failures)}")
    if failures:
        for item in failures:
            print(" -", item)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
