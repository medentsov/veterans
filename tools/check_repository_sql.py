#!/usr/bin/env python3
"""Проверка SQL из PersonRepository.cpp без Qt.

Повторяет те же операторы на реальной схеме (ресурсы resources/db),
чтобы поймать опечатки в SQL до сборки под Windows.

Запуск: python3 tools/check_repository_sql.py
"""

import re
import sqlite3
import sys
from pathlib import Path

DB_DIR = Path(__file__).resolve().parent.parent / "resources" / "db"

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


def build():
    con = sqlite3.connect(":memory:")
    con.execute("PRAGMA foreign_keys = ON")
    for version, path in enumerate(sorted(DB_DIR.glob("*.sql")), start=1):
        for statement in statements_of(path):
            con.execute(statement)
        con.execute(f"PRAGMA user_version = {version}")
    return con


# --- SQL, зеркально PersonRepository.cpp -----------------------------------
SELECT = """
SELECT p.id, p.last_name, p.first_name, p.middle_name, p.birth_date,
       p.microdistrict, p.street, p.house, p.building, p.apartment,
       p.category_id, p.visit_month, p.visit_year,
       p.subscription_id, p.subscription_year,
       p.created_at, p.updated_at,
       c.name AS category_name, s.name AS subscription_name,
       p.phone_type, p.phone
FROM person p
LEFT JOIN category c ON c.id = p.category_id
LEFT JOIN subscription s ON s.id = p.subscription_id
"""

INSERT = """
INSERT INTO person (last_name, first_name, middle_name, birth_date,
                    microdistrict, street, house, building, apartment,
                    category_id, visit_month, visit_year,
                    subscription_id, subscription_year,
                    phone_type, phone,
                    created_at, updated_at)
VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
"""

UPDATE = """
UPDATE person SET
  last_name = ?, first_name = ?, middle_name = ?, birth_date = ?,
  microdistrict = ?, street = ?, house = ?, building = ?, apartment = ?,
  category_id = ?, visit_month = ?, visit_year = ?,
  subscription_id = ?, subscription_year = ?,
  phone_type = ?, phone = ?,
  updated_at = ?
WHERE id = ?
"""


def person_values(suffix="", category=1, subscription=2,
                  phone_type=0, phone="12345"):
    return ("Иванов", "Иван", "Иванович", "1948-03-12", 7, "ул. Ленина",
            "15", "2", "41", category, 3, 2026, subscription, 2026,
            phone_type, phone,
            "2026-11-19 12:00:00", "2026-11-19 12:00:00")


def main():
    con = build()
    con.row_factory = sqlite3.Row

    def insert():
        cur = con.execute(INSERT, person_values())
        if cur.lastrowid != 1:
            raise AssertionError(f"lastrowid={cur.lastrowid}")
        con.commit()

    check("insert: новая запись и id", insert)

    def joined_select():
        row = con.execute(SELECT + " WHERE p.id = ?", (1,)).fetchone()
        assert row is not None, "запись не найдена"
        assert row["category_name"] == "Ветеран труда", row["category_name"]
        assert row["subscription_name"] == "Ветеран", row["subscription_name"]
        assert row["birth_date"] == "1948-03-12"

    check("select: JOIN подставляет названия", joined_select)

    def update_row():
        values = ("Петров",) + person_values()[1:16] + ("2026-11-19 13:00:00",)
        con.execute(UPDATE, values + (1,))
        con.commit()
        row = con.execute("SELECT last_name, updated_at FROM person WHERE id = 1").fetchone()
        assert row["last_name"] == "Петров", row["last_name"]
        assert row["updated_at"] == "2026-11-19 13:00:00"

    check("update: поля и updated_at", update_row)

    def list_filter():
        con.execute(INSERT, person_values())
        con.execute("UPDATE person SET deleted_at = '2026-11-19T14:00:00' WHERE id = 2")
        con.commit()
        rows = con.execute(
            SELECT + " WHERE p.deleted_at IS NULL "
            "ORDER BY p.last_name, p.first_name, p.middle_name").fetchall()
        assert len(rows) == 1, f"в списке {len(rows)} записей"

    check("list: удалённые (deleted_at) не попадают", list_filter)

    def order_by():
        con.execute(INSERT, ("Алексеев",) + person_values()[1:])
        con.commit()
        rows = con.execute(
            SELECT + " WHERE p.deleted_at IS NULL "
            "ORDER BY p.last_name, p.first_name, p.middle_name").fetchall()
        assert rows[0]["last_name"] == "Алексеев", rows[0]["last_name"]

    check("list: сортировка по фамилии", order_by)

    def remove_row():
        con.execute("DELETE FROM person WHERE id = 1")
        con.commit()
        assert con.execute("SELECT count(*) FROM person WHERE id = 1").fetchone()[0] == 0

    check("delete: запись удаляется физически", remove_row)

    def tx_rollback():
        cur = con.execute(INSERT, person_values())
        con.commit()
        pid = cur.lastrowid
        con.execute("BEGIN")
        con.execute("DELETE FROM person WHERE id = ?", (pid,))
        con.rollback()
        left = con.execute("SELECT count(*) FROM person WHERE id = ?",
                           (pid,)).fetchone()[0]
        assert left == 1, "откат не вернул запись"

    check("транзакция: откат сохраняет записи (removeMany)", tx_rollback)

    def tx_commit():
        cur = con.execute(INSERT, person_values())
        con.commit()
        pid = cur.lastrowid
        con.execute("BEGIN")
        con.execute("DELETE FROM person WHERE id = ?", (pid,))
        con.commit()
        left = con.execute("SELECT count(*) FROM person WHERE id = ?",
                           (pid,)).fetchone()[0]
        assert left == 0, "фиксация не удалила запись"

    check("транзакция: фиксация удаляет записи (removeMany)", tx_commit)

    def dicts():
        cats = [r["name"] for r in con.execute(
            "SELECT id, name FROM category ORDER BY sort_order, name")]
        subs = [r["name"] for r in con.execute(
            "SELECT id, name FROM subscription ORDER BY sort_order, name")]
        assert len(cats) == 9 and cats[0] == "Ветеран труда", cats
        assert subs == ["Железногорские новости", "Ветеран"], subs

    check("справочники: порядок и состав", dicts)

    def null_optionals():
        values = person_values(category=None, subscription=None)
        values = values[:9] + (None, None, None, None, None) + values[14:]
        con.execute(INSERT, values)
        con.commit()
        row = con.execute(
            "SELECT p.category_id, s.name AS subscription_name"
            " FROM person p LEFT JOIN subscription s ON s.id = p.subscription_id"
            " WHERE p.id = (SELECT max(id) FROM person)").fetchone()
        assert row["category_id"] is None
        assert row["subscription_name"] is None

    check("null: необязательные поля сохраняются как NULL", null_optionals)

    def phone_roundtrip():
        cur = con.execute(INSERT, person_values(phone_type=1, phone="70000000000"))
        con.commit()
        row = con.execute(SELECT + " WHERE p.id = ?", (cur.lastrowid,)).fetchone()
        assert row["phone_type"] == 1, row["phone_type"]
        assert row["phone"] == "70000000000", row["phone"]

    check("телефон: мобильный сохраняется и читается", phone_roundtrip)

    con.close()

    print(f"\nПроверок: {checks}, провалов: {len(failures)}")
    if failures:
        for item in failures:
            print(" -", item)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
