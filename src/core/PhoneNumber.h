#pragma once

#include <QString>

/// Тип телефона. Значения совпадают с phone_type в базе.
enum class PhoneType {
    Home = 0,    ///< домашний: 5 цифр, на экране X-XX-XX
    Mobile = 1,  ///< мобильный: 11 цифр на 7, на экране +7XXXXXXXXXX
};

/// Нормализация, проверка и форматирование телефона.
///
/// В базе хранятся только цифры; разделители добавляются при показе.
/// Пустая строка означает «телефон не указан» и допускается.
class PhoneNumber
{
public:
    static QString digitsOnly(const QString &input);

    /// Цифры в каноническом виде: для мобильного 8XXXXXXXXXX → 7XXXXXXXXXX,
    /// 9XXXXXXXXX → 79XXXXXXXXX.
    static QString normalize(PhoneType type, const QString &input);

    /// Пусто — допустимо; домашний — ровно 5 цифр; мобильный — 11 цифр на 7.
    static bool isValid(PhoneType type, const QString &input);

    /// Готовый для экрана/печати вид: «X-XX-XX», «+7XXXXXXXXXX» или пусто.
    static QString format(PhoneType type, const QString &stored);

    static QString typeName(PhoneType type);
};
