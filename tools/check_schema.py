#!/usr/bin/env python3
"""Проверка схемы и сида БД без Qt.

Создаёт базу в памяти, применяет все миграции из resources/db по порядку
и проверяет ограничения и идемпотентность сида.

Запуск: python3 tools/check_schema.py

Повторяет логику Migrations.cpp: вырезание комментариев `--` и разбиение по `;`.
"""

import re
import sqlite3
import sys
from pathlib import Path

DB_DIR = Path(__file__).resolve().parent.parent / "resources" / "db"
MIGRATIONS = sorted(DB_DIR.glob("*.sql"))

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
    sql = path.read_text(encoding="utf-8")
    sql = re.sub(r"--.*", "", sql)
    return [s.strip() for s in sql.split(";") if s.strip()]


def apply(con, path):
    for statement in statements_of(path):
        con.execute(statement)


def build():
    con = sqlite3.connect(":memory:")
    con.execute("PRAGMA foreign_keys = ON")
    for version, path in enumerate(MIGRATIONS, start=1):
        apply(con, path)
        con.execute(f"PRAGMA user_version = {version}")
    return con


def expect_error(con, sql, params=()):
    try:
        con.execute(sql, params)
    except sqlite3.IntegrityError:
        con.rollback()
        return
    raise AssertionError("ограничение не сработало")


def count(con, table):
    return con.execute(f"SELECT count(*) FROM {table}").fetchone()[0]


def main():
    if not MIGRATIONS:
        print(f"нет миграций в {DB_DIR}")
        return 1

    con = build()

    check("все таблицы созданы", lambda: (
        con.execute("SELECT count(*) FROM sqlite_master "
                    "WHERE type='table' AND name IN "
                    "('category','subscription','person')").fetchone()[0] == 3
        or (_ for _ in ()).throw(AssertionError("не все таблицы"))))

    check("индексы созданы (6)", lambda: (
        con.execute("SELECT count(*) FROM sqlite_master WHERE type='index' "
                    "AND name LIKE 'idx_person_%'").fetchone()[0] == 6
        or (_ for _ in ()).throw(AssertionError("индексов не 6"))))

    check("резерв корзины: поле deleted_at", lambda: (
        any(row[1] == "deleted_at"
            for row in con.execute("PRAGMA table_info(person)"))
        or (_ for _ in ()).throw(AssertionError("нет поля deleted_at"))))

    check(f"версия схемы = {len(MIGRATIONS)}", lambda: (
        con.execute("PRAGMA user_version").fetchone()[0] == len(MIGRATIONS)
        or (_ for _ in ()).throw(AssertionError("неверная user_version"))))

    check("категорий 9", lambda: count(con, "category") == 9
          or (_ for _ in ()).throw(AssertionError(f"категорий {count(con, 'category')}")))
    check("подписок 2", lambda: count(con, "subscription") == 2
          or (_ for _ in ()).throw(AssertionError(f"подписок {count(con, 'subscription')}")))

    check("названия категорий из ТЗ", lambda: (
        [row[0] for row in con.execute(
            "SELECT name FROM category ORDER BY sort_order")] == [
            "Ветеран труда", "Инвалид", "Участник боевых действий",
            "Участник СВО", "Член семьи СВО", "Ликвидатор ЧАЭС",
            "Несовершеннолетний узник", "Дети войны", "Без льгот"]
        or (_ for _ in ()).throw(AssertionError("названия не совпадают"))))

    def seed_twice():
        seed = next(p for p in MIGRATIONS if p.name == "002_seed.sql")
        apply(con, seed)
        apply(con, seed)
        if count(con, "category") != 9 or count(con, "subscription") != 2:
            raise AssertionError("сид создал дубликаты")

    check("сид идемпотентен (повторный запуск)", seed_twice)

    person_sql = """
        INSERT INTO person (last_name, first_name, middle_name, birth_date,
                            microdistrict, street, house, building, apartment,
                            category_id, visit_month, visit_year,
                            subscription_id, subscription_year,
                            created_at, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    """
    valid = ("Иванов", "Иван", "Иванович", "1948-03-12", 7, "ул. Ленина",
             "15", "2", "41", 1, 3, 2026, 1, 2026,
             "2026-11-19 12:00:00", "2026-11-19 12:00:00")

    def insert_valid():
        con.execute(person_sql, valid)
        con.commit()

    check("корректная запись вставляется", insert_valid)

    check("микрорайон 0 отклоняется",
          lambda: expect_error(con, person_sql, valid[:4] + (0,) + valid[5:]))
    check("микрорайон 1000 отклоняется",
          lambda: expect_error(con, person_sql, valid[:4] + (1000,) + valid[5:]))
    check("дата рождения не в ISO отклоняется",
          lambda: expect_error(con, person_sql, valid[:3] + ("12.03.1948",) + valid[4:]))
    check("месяц посещения 13 отклоняется",
          lambda: expect_error(con, person_sql,
                               valid[:10] + (13,) + valid[11:]))
    check("месяц без года отклоняется",
          lambda: expect_error(con, person_sql,
                               valid[:10] + (3, None) + valid[12:]))
    check("год посещения 1800 отклоняется",
          lambda: expect_error(con, person_sql,
                               valid[:11] + (1800,) + valid[12:]))
    check("подписка без года отклоняется",
          lambda: expect_error(con, person_sql, valid[:13] + (None,) + valid[14:]))
    check("год подписки без газеты отклоняется",
          lambda: expect_error(con, person_sql, valid[:12] + (None, 2026) + valid[14:]))
    check("несуществующая категория отклоняется",
          lambda: expect_error(con, person_sql, valid[:9] + (99,) + valid[10:]))
    check("удаление используемой категории отклоняется",
          lambda: expect_error(con, "DELETE FROM category WHERE id = 1"))

    con.execute("UPDATE person SET phone = '12345', phone_type = 0")
    con.commit()

    check("домашний телефон 5 цифр принимается",
          lambda: (con.execute("UPDATE person SET phone = '12345', phone_type = 0")
                   or con.commit() or True))

    check("домашний телефон из 6 цифр отклоняется",
          lambda: expect_error(con, "UPDATE person SET phone = '123456' WHERE id = 1"))

    check("мобильный телефон принимается",
          lambda: (con.execute("UPDATE person SET phone = '70000000000', phone_type = 1")
                   or con.commit() or True))

    check("мобильный с 5 цифрами отклоняется",
          lambda: expect_error(con, "UPDATE person SET phone = '12345', phone_type = 1"))

    check("пустой телефон допускается",
          lambda: (con.execute("UPDATE person SET phone = '' WHERE id = 1")
                   or con.commit() or True))

    con.close()

    print(f"\nМиграций: {len(MIGRATIONS)}, проверок: {checks}, "
          f"провалов: {len(failures)}")
    if failures:
        print("Провалы:")
        for item in failures:
            print(" -", item)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
