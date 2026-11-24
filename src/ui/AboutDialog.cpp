#include "ui/AboutDialog.h"

#include "core/version.h"

#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtGlobal>

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("О программе"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/veterans-64.png")));
    setModal(true);

    auto *icon = new QLabel(this);
    icon->setPixmap(QIcon(QStringLiteral(":/icons/veterans-256.png")).pixmap(72, 72));
    icon->setAlignment(Qt::AlignCenter);

    auto *title = new QLabel(QStringLiteral("Ветераны"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 6);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setAlignment(Qt::AlignCenter);

    auto *version = new QLabel(
        tr("Версия %1").arg(QStringLiteral(APP_VERSION_STRING)), this);
    version->setAlignment(Qt::AlignCenter);

    auto *description = new QLabel(
        tr("Справочник ветеранов госпиталя.\n"
           "Данные хранятся локально, подключение к сети не требуется."),
        this);
    description->setAlignment(Qt::AlignCenter);
    description->setWordWrap(true);

    auto *qtVersion = new QLabel(
        tr("Работает на Qt %1 (LGPLv3)").arg(QString::fromLatin1(qVersion())), this);
    qtVersion->setAlignment(Qt::AlignCenter);
    QPalette muted = qtVersion->palette();
    muted.setColor(QPalette::WindowText,
                   muted.color(QPalette::Disabled, QPalette::WindowText));
    qtVersion->setPalette(muted);

    auto *closeButton = new QPushButton(tr("Закрыть"), this);
    closeButton->setDefault(true);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch();
    buttonRow->addWidget(closeButton);
    buttonRow->addStretch();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 20);
    layout->setSpacing(10);
    layout->addWidget(icon);
    layout->addWidget(title);
    layout->addWidget(version);
    layout->addSpacing(6);
    layout->addWidget(description);
    layout->addSpacing(10);
    layout->addWidget(qtVersion);
    layout->addLayout(buttonRow);

    setFixedWidth(400);
}
