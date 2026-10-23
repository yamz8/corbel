#include "report.h"

#include "pin.h"
#include "store.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QHash>
#include <QLocale>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QSaveFile>

namespace {
QString tr(const char *text) { return QCoreApplication::translate("Report", text); }

constexpr int Dpi = 150;
const QColor Ink(0x1d, 0x1d, 0x1f);
const QColor Faint(0x6e, 0x6e, 0x73);
const QColor Rule(0xd2, 0xd2, 0xd7);

// A page being laid out from top to bottom.
struct Page {
    QPdfWriter &writer;
    QPainter &painter;
    QRectF area;
    qreal y = 0;
    bool fresh = true;

    void next() {
        if (!fresh)
            writer.newPage();
        fresh = false;
        y = area.top();
    }
    bool fits(qreal height) const { return y + height <= area.bottom(); }
};

QFont font(qreal points, bool bold = false) {
    QFont f;
    f.setPointSizeF(points);
    f.setBold(bold);
    return f;
}

QString dueText(const QDate &due) {
    return due.isValid() ? QLocale().toString(due, QLocale::ShortFormat) : QString();
}
}

QColor statusPrintColor(int status) {
    switch (status) {
    case int(Status::InProgress): return QColor(0xf5, 0xa5, 0x24);
    case int(Status::Done): return QColor(0x30, 0xa4, 0x6c);
    default: return QColor(0xe5, 0x48, 0x4d);
    }
}

QString statusLabel(int status) {
    switch (status) {
    case int(Status::InProgress): return QCoreApplication::translate("Status", "In progress");
    case int(Status::Done): return QCoreApplication::translate("Status", "Done");
    default: return QCoreApplication::translate("Status", "Open");
    }
}

bool writeReport(const Store &store, const QString &path, QString *error) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        *error = tr("Could not write %1: %2").arg(path, file.errorString());
        return false;
    }

    const QString projectName = store.name();
    const QList<Plan> plans = store.plans();
    const QList<Item> items = store.items();
    QHash<int, QString> planNames;
    for (const auto &plan : plans)
        planNames.insert(plan.id, plan.name);

    {
        QPdfWriter writer(&file);
        writer.setTitle(tr("%1 punch list").arg(projectName));
        writer.setCreator("Corbel");
        writer.setResolution(Dpi);
        writer.setPageLayout(QPageLayout(QPageSize(QPageSize::A4), QPageLayout::Landscape, QMarginsF(14, 14, 14, 14),
                                         QPageLayout::Millimeter));
        QPainter painter(&writer);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        Page page{writer, painter, QRectF(QPointF(0, 0), writer.pageLayout().paintRectPixels(Dpi).size())};
        const qreal width = page.area.width();
        const bool rtl = QLocale().textDirection() == Qt::RightToLeft;
        const int lead = rtl ? Qt::AlignRight : Qt::AlignLeft;

        auto text = [&](const QRectF &rect, const QString &value, const QFont &f, const QColor &color, int flags) {
            painter.setFont(f);
            painter.setPen(color);
            painter.drawText(rect, flags | Qt::AlignVCenter | Qt::TextSingleLine,
                             QFontMetricsF(f, &writer).elidedText(value, Qt::ElideRight, rect.width()));
        };

        // Summary.
        page.next();
        text(QRectF(0, page.y, width, 60), tr("%1 punch list").arg(projectName), font(22, true), Ink, lead);
        page.y += 64;
        int counts[3] = {0, 0, 0};
        for (const auto &item : items)
            counts[qBound(0, item.status, 2)]++;
        QString summary = QLocale().toString(QDate::currentDate(), QLocale::LongFormat) + " · " +
                          QCoreApplication::translate("Report", "%n items", nullptr, items.size());
        for (int status = 0; status < 3; ++status)
            summary += QString(" · %1 %2").arg(statusLabel(status)).arg(QLocale().toString(counts[status]));
        text(QRectF(0, page.y, width, 30), summary, font(10), Faint, lead);
        page.y += 50;

        // The item table, with its header repeated on every page.
        struct Column { QString title; qreal share; };
        const QList<Column> columns = {{"#", .05},          {tr("Item"), .33},     {tr("Floor"), .16},
                                       {tr("Trade"), .13},  {tr("Assignee"), .13}, {tr("Due"), .09},
                                       {tr("Status"), .11}};
        const qreal rowHeight = 34;
        auto columnRect = [&](int index, qreal top) {
            qreal start = 0;
            for (int i = 0; i < index; ++i)
                start += columns[i].share * width;
            const qreal w = columns[index].share * width;
            return QRectF(rtl ? width - start - w : start, top, w - 10, rowHeight);
        };
        auto header = [&] {
            for (int i = 0; i < columns.size(); ++i)
                text(columnRect(i, page.y), columns[i].title, font(9, true), Faint, lead);
            page.y += rowHeight;
            painter.setPen(QPen(Ink, 2));
            painter.drawLine(QPointF(0, page.y), QPointF(width, page.y));
        };
        header();
        if (items.isEmpty()) {
            text(QRectF(0, page.y + 10, width, rowHeight), tr("No items yet."), font(10), Faint, lead);
            page.y += rowHeight + 10;
        }
        for (const auto &item : items) {
            if (!page.fits(rowHeight)) {
                page.next();
                header();
            }
            const QStringList cells = {QLocale().toString(item.number), item.title, planNames.value(item.planId),
                                       item.trade, item.assignee, dueText(item.due), statusLabel(item.status)};
            for (int i = 0; i < cells.size(); ++i) {
                QRectF rect = columnRect(i, page.y);
                if (i == cells.size() - 1) {
                    const qreal dot = 14;
                    const qreal dotX = rtl ? rect.right() - dot : rect.left();
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(statusPrintColor(item.status));
                    painter.drawEllipse(QRectF(dotX, rect.center().y() - dot / 2, dot, dot));
                    rect.adjust(rtl ? 0 : dot + 8, 0, rtl ? -dot - 8 : 0, 0);
                }
                text(rect, cells[i], font(10, i == 1), Ink, lead);
            }
            page.y += rowHeight;
            painter.setPen(QPen(Rule, 1));
            painter.drawLine(QPointF(0, page.y), QPointF(width, page.y));
        }

        // Each plan on its own page with its pins.
        for (const auto &plan : plans) {
            page.next();
            text(QRectF(0, page.y, width, 44), plan.name, font(16, true), Ink, lead);
            page.y += 54;
            const QImage image = store.planImage(plan.id);
            if (image.isNull())
                continue;
            QSizeF size = QSizeF(image.size()).scaled(QSizeF(width, page.area.bottom() - page.y), Qt::KeepAspectRatio);
            const QRectF target((width - size.width()) / 2, page.y, size.width(), size.height());
            painter.drawImage(target, image);
            painter.setPen(QPen(Rule, 1));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(target);
            const qreal diameter = qBound(26.0, qMin(target.width(), target.height()) / 20, 46.0);
            for (const auto &item : items)
                if (item.planId == plan.id)
                    paintPin(&painter, QPointF(target.left() + item.x * target.width(), target.top() + item.y * target.height()),
                             diameter, item.number, statusPrintColor(item.status), false);
        }

        // Photos, a row of three per item that has any.
        bool photosStarted = false;
        const qreal gap = 16, photoWidth = (width - 2 * gap) / 3, photoHeight = photoWidth * .75;
        for (const auto &item : items) {
            const QList<Photo> photos = store.photos(item.id);
            if (photos.isEmpty())
                continue;
            const int rows = (photos.size() + 2) / 3;
            if (!photosStarted || !page.fits(40 + photoHeight + gap)) {
                page.next();
                if (!photosStarted)
                    text(QRectF(0, page.y, width, 44), tr("Photos"), font(16, true), Ink, lead);
                page.y += photosStarted ? 0 : 54;
                photosStarted = true;
            }
            text(QRectF(0, page.y, width, 34), QString("%1  %2").arg(QLocale().toString(item.number), item.title),
                 font(11, true), Ink, lead);
            page.y += 40;
            for (int row = 0; row < rows; ++row) {
                if (!page.fits(photoHeight + gap))
                    page.next();
                for (int column = 0; column < 3 && row * 3 + column < photos.size(); ++column) {
                    QImage photo = QImage::fromData(store.photoData(photos[row * 3 + column].id));
                    if (photo.isNull())
                        continue;
                    photo = photo.scaled(QSize(900, 900), Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    const QSizeF size = QSizeF(photo.size()).scaled(QSizeF(photoWidth, photoHeight), Qt::KeepAspectRatio);
                    const qreal cellX = column * (photoWidth + gap);
                    const qreal x = rtl ? width - cellX - photoWidth : cellX;
                    painter.drawImage(QRectF(x, page.y, size.width(), size.height()), photo);
                }
                page.y += photoHeight + gap;
            }
        }
        if (!painter.end()) {
            file.cancelWriting();
            *error = tr("Could not write %1.").arg(path);
            return false;
        }
    }

    if (!file.commit()) {
        *error = tr("Could not write %1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}
