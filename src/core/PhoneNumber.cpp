#include "core/PhoneNumber.h"

QString PhoneNumber::digitsOnly(const QString &input)
{
    QString digits;
    digits.reserve(input.size());
    for (const QChar ch : input) {
        if (ch.isDigit())
            digits.append(ch);
    }
    return digits;
}

QString PhoneNumber::normalize(PhoneType type, const QString &input)
{
    QString digits = digitsOnly(input);
    if (digits.isEmpty())
        return {};

    if (type == PhoneType::Mobile) {
        if (digits.size() == 11 && digits.startsWith(QLatin1Char('8')))
            digits[0] = QLatin1Char('7');
        else if (digits.size() == 10 && digits.startsWith(QLatin1Char('9')))
            digits.prepend(QLatin1Char('7'));
    }
    return digits;
}

bool PhoneNumber::isValid(PhoneType type, const QString &input)
{
    const QString digits = normalize(type, input);
    if (digits.isEmpty())
        return true;   // телефон необязателен

    if (type == PhoneType::Home)
        return digits.size() == 5;

    return digits.size() == 11 && digits.startsWith(QLatin1Char('7'));
}

QString PhoneNumber::format(PhoneType type, const QString &stored)
{
    const QString digits = digitsOnly(stored);
    if (digits.isEmpty())
        return {};

    if (type == PhoneType::Home) {
        if (digits.size() == 5) {
            return QStringLiteral("%1-%2-%3")
                .arg(digits.mid(0, 1), digits.mid(1, 2), digits.mid(3, 2));
        }
        return digits;
    }

    return QLatin1Char('+') + digits;
}

QString PhoneNumber::typeName(PhoneType type)
{
    return type == PhoneType::Home ? QStringLiteral("домашний")
                                   : QStringLiteral("мобильный");
}
