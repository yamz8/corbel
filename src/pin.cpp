#include "pin.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <cmath>

namespace {
// Black or white, whichever reads on the fill (WCAG relative luminance).
QColor readableOn(const QColor &c) {
    auto linear = [](double v) { return v <= .04045 ? v / 12.92 : std::pow((v + .055) / 1.055, 2.4); };
    const double luminance = .2126 * linear(c.redF()) + .7152 * linear(c.greenF()) + .0722 * linear(c.blueF());
    return luminance > .179 ? Qt::black : Qt::white;
}
}

void paintPin(QPainter *painter, const QPointF &center, qreal diameter, int number, const QColor &fill,
              bool selected) {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const qreal radius = diameter / 2;

    // A dark halo keeps the pin visible on both white paper and dark plans.
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(0, 0, 0, 90));
    painter->drawEllipse(center, radius + diameter * .08, radius + diameter * .08);

    painter->setBrush(fill);
    painter->setPen(QPen(selected ? QColor(Qt::white) : fill.darker(150), diameter * (selected ? .14 : .07)));
    painter->drawEllipse(center, radius, radius);

    // The digits are drawn as a path scaled to the circle, so they fill it
    // the same on screen and in a PDF, whose font metrics differ.
    QFont font;
    font.setBold(true);
    font.setPixelSize(100);
    QPainterPath digits;
    digits.addText(0, 0, font, QString::number(number));
    const QRectF bounds = digits.boundingRect();
    if (!bounds.isEmpty()) {
        const qreal scale = qMin(diameter * .4 / bounds.height(), diameter * .64 / bounds.width());
        QTransform transform;
        transform.translate(center.x(), center.y());
        transform.scale(scale, scale);
        transform.translate(-bounds.center().x(), -bounds.center().y());
        painter->fillPath(transform.map(digits), readableOn(fill));
    }
    painter->restore();
}

PinItem::PinItem(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true);
    connect(this, &PinItem::changed, this, [this] { update(); });
}

void PinItem::setNumber(int number) {
    if (number == m_number)
        return;
    m_number = number;
    emit changed();
}

void PinItem::setColor(const QColor &color) {
    if (color == m_color)
        return;
    m_color = color;
    emit changed();
}

void PinItem::setSelected(bool selected) {
    if (selected == m_selected)
        return;
    m_selected = selected;
    emit changed();
}

void PinItem::paint(QPainter *painter) {
    // Leave room for the halo inside the item's bounds.
    const qreal diameter = qMin(width(), height()) / 1.2;
    paintPin(painter, boundingRect().center(), diameter, m_number, m_color, m_selected);
}
