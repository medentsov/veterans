-- Справочники, версия 2. Миграция идемпотентна: повторный запуск не создаёт
-- дубликатов, а приводит названия и порядок к значениям из ТЗ.
-- ID зафиксированы, потому что на них ссылаются записи person.

INSERT INTO category (id, name, sort_order) VALUES
    (1, 'Ветеран труда',              1),
    (2, 'Инвалид',                    2),
    (3, 'Участник боевых действий',   3),
    (4, 'Участник СВО',               4),
    (5, 'Член семьи СВО',             5),
    (6, 'Ликвидатор ЧАЭС',            6),
    (7, 'Несовершеннолетний узник',   7),
    (8, 'Дети войны',                 8),
    (9, 'Без льгот',                  9)
ON CONFLICT(id) DO UPDATE SET
    name       = excluded.name,
    sort_order = excluded.sort_order;

INSERT INTO subscription (id, name, sort_order) VALUES
    (1, 'Железногорские новости', 1),
    (2, 'Ветеран',                2)
ON CONFLICT(id) DO UPDATE SET
    name       = excluded.name,
    sort_order = excluded.sort_order;
