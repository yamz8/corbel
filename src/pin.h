#pragma once

#include <QColor>
#include <QQuickPaintedItem>

class QPainter;

// A pin is painted by this one function on screen and in the report, so the
// numbered markers on a printed plan look exactly like the ones in the app.
void paintPin(QPainter *painter, const QPointF &center, qreal diameter, int number, const QColor &fill,
              bool selected);

// The pin as a Qt Quick item, centered on the spot it marks.
class PinItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(int number READ number WRITE setNumber NOTIFY changed)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY changed)
    Q_PROPERTY(bool selected READ selected WRITE setSelected NOTIFY changed)

public:
    explicit PinItem(QQuickItem *parent = nullptr);

    int number() const { return m_number; }
    void setNumber(int number);
    QColor color() const { return m_color; }
    void setColor(const QColor &color);
    bool selected() const { return m_selected; }
    void setSelected(bool selected);

    void paint(QPainter *painter) override;

signals:
    void changed();

private:
    int m_number = 0;
    QColor m_color = Qt::red;
    bool m_selected = false;
};
