-- Схема БД «Ветераны», версия 1.
-- Применяется один раз; номер версии хранится в PRAGMA user_version.
-- Справочники заполняются отдельной миграцией (M2-02).

CREATE TABLE category (
    id         INTEGER PRIMARY KEY,
    name       TEXT NOT NULL UNIQUE,
    sort_order INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE subscription (
    id         INTEGER PRIMARY KEY,
    name       TEXT NOT NULL UNIQUE,
    sort_order INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE person (
    id                INTEGER PRIMARY KEY AUTOINCREMENT,
    last_name         TEXT    NOT NULL,
    first_name        TEXT    NOT NULL,
    middle_name       TEXT    NOT NULL DEFAULT '',
    birth_date        TEXT    NOT NULL
                      CHECK (birth_date GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]'),
    microdistrict     INTEGER NOT NULL
                      CHECK (microdistrict BETWEEN 1 AND 999),
    street            TEXT    NOT NULL DEFAULT '',
    house             TEXT    NOT NULL DEFAULT '',
    building          TEXT    NOT NULL DEFAULT '',
    apartment         TEXT    NOT NULL DEFAULT '',
    category_id       INTEGER REFERENCES category(id),
    visit_month       INTEGER CHECK (visit_month IS NULL OR visit_month BETWEEN 1 AND 12),
    visit_year        INTEGER CHECK (visit_year IS NULL OR visit_year BETWEEN 1990 AND 2100),
    subscription_id   INTEGER REFERENCES subscription(id),
    subscription_year INTEGER CHECK (subscription_year IS NULL OR subscription_year BETWEEN 2000 AND 2100),
    created_at        TEXT    NOT NULL,
    updated_at        TEXT    NOT NULL,
    deleted_at        TEXT,

    CHECK ((visit_month IS NULL AND visit_year IS NULL)
           OR (visit_month IS NOT NULL AND visit_year IS NOT NULL)),
    CHECK ((subscription_id IS NULL AND subscription_year IS NULL)
           OR (subscription_id IS NOT NULL AND subscription_year IS NOT NULL))
);

CREATE INDEX idx_person_name     ON person(last_name, first_name);
CREATE INDEX idx_person_birth    ON person(birth_date);
CREATE INDEX idx_person_micro    ON person(microdistrict);
CREATE INDEX idx_person_visit    ON person(visit_year);
CREATE INDEX idx_person_category ON person(category_id);
CREATE INDEX idx_person_sub      ON person(subscription_id);
