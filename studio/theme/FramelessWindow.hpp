#pragma once

#include <QByteArray>
#include <QtTypes>

class QWidget;

namespace aur
{

    bool handleFramelessNativeEvent(QWidget *window, const QByteArray &eventType, void *message,
                                    qintptr *result);

}
