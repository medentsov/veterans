-- Версия 3: телефон ветерана.
--
-- phone_type: 0 — домашний, 1 — мобильный.
-- phone хранится в нормализованном виде: пусто | 5 цифр | 11 цифр, начиная с 7.
-- Формат на экране и в печати: X-XX-XX / +7XXXXXXXXXX, разделители не хранятся.

ALTER TABLE person ADD COLUMN phone_type INTEGER NOT NULL DEFAULT 0
    CHECK (phone_type IN (0, 1));

ALTER TABLE person ADD COLUMN phone TEXT NOT NULL DEFAULT ''
    CHECK (phone = ''
           OR (phone_type = 0
               AND phone GLOB '[0-9][0-9][0-9][0-9][0-9]')
           OR (phone_type = 1
               AND phone GLOB '7[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]'));
