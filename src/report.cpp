#include "report.h"

#include "pin.h"

#include <QBuffer>
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

QFont font(qreal points, bool bold = false) {
    QFont f;
    f.setPointSizeF(points);
    f.setBold(bold);
    return f;
}

// A page being laid out from top to bottom, with the report's name and the
// page number along its foot.
struct Page {
    QPdfWriter &writer;
    QPainter &painter;
    QRectF area;
    QString name;
    int total = 0;
    int number = 0;
    qreal y = 0;

    static constexpr qreal Foot = 40;

    void next() {
        if (number)
            writer.newPage();
        ++number;
        y = area.top();
        const QRectF foot(area.left(), area.bottom() - Foot + 12, area.width(), Foot - 12);
        const bool rtl = QLocale().textDirection() == Qt::RightToLeft;
        painter.setFont(font(8));
        painter.setPen(Faint);
        painter.drawText(foot, (rtl ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter, name);
        if (total)
            painter.drawText(foot, (rtl ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter,
                             tr("Page %1 of %2").arg(QLocale().toString(number), QLocale().toString(total)));
    }
    bool fits(qreal height) const { return y + height <= area.bottom() - Foot; }
};

QString dueText(const QDate &due) {
    return due.isValid() ? QLocale().toString(due, QLocale::ShortFormat) : QString();
}

bool overdue(const Item &item) {
    return item.due.isValid() && item.due < QDate::currentDate() && item.status != int(Status::Done);
}

// Lays the report out on the writer and returns how many pages it took. The
// page count isn't known until the end, so the report is laid out once to
// count its pages and again to number them; counting leaves the images out,
// since they never change where a page breaks.
int render(QPdfWriter &writer, const Store &store, const QList<Item> &items, const QList<Plan> &plans,
           const ItemFilter &filter, int total);
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

QString filterDescription(const Store &store, const ItemFilter &filter) {
    QStringList parts;
    if (filter.status >= 0)
        parts << statusLabel(filter.status);
    if (!filter.trade.isEmpty())
        parts << filter.trade;
    if (!filter.assignee.isEmpty())
        parts << filter.assignee;
    if (filter.floor)
        parts << store.plan(filter.floor).name;
    return parts.join(" · ");
}

bool writeReport(const Store &store, const QString &path, const ItemFilter &filter, QString *error) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        *error = tr("Could not write %1: %2").arg(path, file.errorString());
        return false;
    }

    QList<Item> items;
    for (const auto &item : store.items())
        if (filter.matches(item))
            items << item;
    // A filtered report shows only the floors its items are on.
    QList<Plan> plans;
    for (const auto &plan : store.plans()) {
        bool used = filter.isEmpty();
        for (const auto &item : items)
            used = used || item.planId == plan.id;
        if (used)
            plans << plan;
    }

    // A first pass counts the pages, so every page can say "of" how many.
    QBuffer scratch;
    scratch.open(QIODevice::WriteOnly);
    int total;
    {
        QPdfWriter counter(&scratch);
        total = render(counter, store, items, plans, filter, 0);
    }
    {
        QPdfWriter writer(&file);
        if (render(writer, store, items, plans, filter, total) < 0) {
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

namespace {
int render(QPdfWriter &writer, const Store &store, const QList<Item> &items, const QList<Plan> &plans,
           const ItemFilter &filter, int total) {
    const QString projectName = store.name();
    QHash<int, QString> planNames;
    for (const auto &plan : plans)
        planNames.insert(plan.id, plan.name);

    writer.setTitle(tr("%1 punch list").arg(projectName));
    writer.setCreator("Corbel");
    writer.setResolution(Dpi);
    writer.setPageLayout(QPageLayout(QPageSize(QPageSize::A4), QPageLayout::Landscape, QMarginsF(14, 14, 14, 14),
                                     QPageLayout::Millimeter));
    QPainter painter(&writer);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    Page page{writer, painter, QRectF(QPointF(0, 0), writer.pageLayout().paintRectPixels(Dpi).size()),
              tr("%1 punch list").arg(projectName), total};
    const qreal width = page.area.width();
    const bool rtl = QLocale().textDirection() == Qt::RightToLeft;
    const int lead = rtl ? Qt::AlignRight : Qt::AlignLeft;
    const bool counting = total == 0;

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
    page.y += 34;
    if (!filter.isEmpty()) {
        text(QRectF(0, page.y, width, 30), tr("Only %1").arg(filterDescription(store, filter)), font(10, true), Ink, lead);
        page.y += 34;
    }
    page.y += 16;

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
        text(QRectF(0, page.y + 10, width, rowHeight), filter.isEmpty() ? tr("No items yet.") : tr("No items match."),
             font(10), Faint, lead);
        page.y += rowHeight + 10;
    }
    // Notes run under their item across the columns up to the due date, a
    // dozen whole lines at most so one long note can't push a row off the page.
    const QFont noteFont = font(9);
    const QFontMetricsF noteMetrics(noteFont, &writer);
    const QRectF noteSpan = columnRect(1, 0).united(columnRect(4, 0));
    auto notesHeight = [&](const Item &item) -> qreal {
        if (item.notes.trimmed().isEmpty())
            return 0;
        const QRectF bound = noteMetrics.boundingRect(QRectF(0, 0, noteSpan.width(), 1e6),
                                                      Qt::TextWordWrap, item.notes.trimmed());
        return qMin(bound.height(), noteMetrics.lineSpacing() * 12);
    };
    for (const auto &item : items) {
        const qreal notes = notesHeight(item);
        if (!page.fits(rowHeight + notes + 10)) {
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
            const bool late = i == 5 && overdue(item);
            text(rect, cells[i], font(10, i == 1 || late), late ? statusPrintColor(int(Status::Open)) : Ink, lead);
        }
        page.y += rowHeight;
        if (notes > 0) {
            const QRectF rect(noteSpan.left(), page.y - 6, noteSpan.width(), notes);
            painter.save();
            painter.setClipRect(rect);
            painter.setFont(noteFont);
            painter.setPen(Faint);
            painter.drawText(rect, lead | Qt::AlignTop | Qt::TextWordWrap, item.notes.trimmed());
            painter.restore();
            page.y += notes + 10;
        }
        painter.setPen(QPen(Rule, 1));
        painter.drawLine(QPointF(0, page.y), QPointF(width, page.y));
    }

    // Each plan on its own page with its pins.
    for (const auto &plan : plans) {
        page.next();
        text(QRectF(0, page.y, width, 44), plan.name, font(16, true), Ink, lead);
        page.y += 54;
        const QImage image = counting ? QImage() : store.planImage(plan.id);
        if (image.isNull())
            continue;
        QSizeF size = QSizeF(image.size()).scaled(QSizeF(width, page.area.bottom() - Page::Foot - page.y), Qt::KeepAspectRatio);
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
                if (counting)
                    continue;
                QImage photo = QImage::fromData(store.photoData(photos[row * 3 + column].id));
                if (photo.isNull())
                    continue;
                // At the cell's own size the print is as sharp as it gets, and a
                // report of a few hundred photos stays small enough to email.
                photo = photo.scaled(QSizeF(photoWidth, photoHeight).toSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
                const QSizeF size = QSizeF(photo.size()).scaled(QSizeF(photoWidth, photoHeight), Qt::KeepAspectRatio);
                const qreal cellX = column * (photoWidth + gap);
                const qreal x = rtl ? width - cellX - photoWidth : cellX;
                painter.drawImage(QRectF(x, page.y, size.width(), size.height()), photo);
            }
            page.y += photoHeight + gap;
        }
    }
    return painter.end() ? page.number : -1;
}
}
