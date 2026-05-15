#pragma once

#include <QColor>
#include <QRgb>
#include <QString>
#include <QVector>

struct DmcColor {
    QString number;
    QString name;
    QColor color;
};

struct DmcMatch {
    bool ok = false;
    DmcColor color;
    int distanceSquared = 0;
};

namespace DmcMatcher {
const QVector<DmcColor> &palette();
int colorDistanceSquared(const QColor &a, const QColor &b);
DmcMatch nearest(QRgb rgba);
DmcMatch nearest(const QColor &color);
}
