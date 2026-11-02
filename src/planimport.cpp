#include "planimport.h"

#include <QBuffer>
#include <QCollator>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>

namespace {
// Large enough to read room numbers on an A1 sheet when zoomed in, small
// enough that a multi-floor project stays quick to open.
constexpr int PlanLongSide = 4000;
constexpr int PhotoLongSide = 2000;

QString tr(const char *text) { return QCoreApplication::translate("Import", text); }

QImage readImage(const QString &path, int longSide, QString *error) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QSize size = reader.size();
    if (size.isValid() && qMax(size.width(), size.height()) > longSide)
        reader.setScaledSize(size.scaled(longSide, longSide, Qt::KeepAspectRatio));
    QImage image = reader.read();
    if (image.isNull())
        *error = tr("%1 could not be read: %2").arg(QFileInfo(path).fileName(), reader.errorString());
    return image;
}

// How many pages the PDF has, from poppler's pdfinfo; 0 when it can't tell.
int pageCount(const QString &path) {
    const QString pdfinfo = QStandardPaths::findExecutable("pdfinfo");
    if (pdfinfo.isEmpty())
        return 0;
    QProcess process;
    process.start(pdfinfo, {path});
    if (!process.waitForFinished(30000) || process.exitCode() != 0)
        return 0;
    const auto match = QRegularExpression("^Pages:\\s+(\\d+)", QRegularExpression::MultilineOption)
                           .match(QString::fromUtf8(process.readAllStandardOutput()));
    return match.hasMatch() ? match.captured(1).toInt() : 0;
}
}

QList<PlanPage> loadPlan(const QString &path, QString *error) {
    const QFileInfo info(path);
    if (!info.isFile()) {
        *error = tr("%1 does not exist.").arg(path);
        return {};
    }
    if (info.suffix().compare("pdf", Qt::CaseInsensitive) != 0) {
        const QImage image = readImage(path, PlanLongSide, error);
        if (image.isNull())
            return {};
        QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.convertToFormat(QImage::Format_RGB32).save(&buffer, "PNG");
    return {{info.completeBaseName(), png, image.size()}};
    }

    const QString pdftoppm = QStandardPaths::findExecutable("pdftoppm");
    if (pdftoppm.isEmpty()) {
        *error = tr("Opening PDF plans needs pdftoppm. Install it with: omarchy-pkg-add poppler");
        return {};
    }
    // A page takes pdftoppm about a second, so a set of drawings is split
    // across the cores, each process rendering its own run of pages.
    QTemporaryDir out;
    const int count = pageCount(path);
    const int workers = qBound(1, qMin(QThread::idealThreadCount(), count), 16);
    const int share = count ? (count + workers - 1) / workers : 0;
    QList<QProcess *> processes;
    for (int worker = 0; worker < workers; ++worker) {
        QStringList arguments = {"-png", "-scale-to", QString::number(PlanLongSide)};
        if (share) {
            const int first = worker * share + 1;
            if (first > count)
                break;
            arguments << "-f" << QString::number(first) << "-l" << QString::number(qMin(count, first + share - 1));
        }
        auto *process = new QProcess;
        process->start(pdftoppm, arguments << path << out.filePath("page"));
        processes << process;
    }
    QString failure;
    for (auto *process : processes) {
        if (!process->waitForFinished(300000) || process->exitStatus() != QProcess::NormalExit || process->exitCode() != 0) {
            process->kill();
            process->waitForFinished();
            const QString detail = QString::fromUtf8(process->readAllStandardError()).trimmed();
            if (failure.isEmpty())
                failure = detail.isEmpty() ? process->errorString() : detail;
        }
        delete process;
    }
    if (!failure.isEmpty()) {
        *error = tr("%1 could not be rendered: %2").arg(info.fileName(), failure);
        return {};
    }

    // pdftoppm pads page numbers to the page count's width, so a natural sort
    // keeps page 10 after page 9.
    QStringList files = QDir(out.path()).entryList({"page*.png"}, QDir::Files);
    QCollator collator;
    collator.setNumericMode(true);
    std::sort(files.begin(), files.end(), collator);

    // pdftoppm's PNGs are already as small as they get, so they are stored
    // as they are rather than decoded and compressed again.
    QList<PlanPage> pages;
    for (int i = 0; i < files.size(); ++i) {
        QFile file(out.filePath(files[i]));
        const QSize size = QImageReader(file.fileName()).size();
        if (!size.isValid() || !file.open(QIODevice::ReadOnly)) {
            *error = tr("%1 could not be rendered: %2").arg(info.fileName(), file.fileName());
            return {};
        }
        const QString name = files.size() == 1 ? info.completeBaseName()
                                               : tr("%1, page %2").arg(info.completeBaseName()).arg(i + 1);
        pages << PlanPage{name, file.readAll(), size};
    }
    if (pages.isEmpty())
        *error = tr("%1 has no pages.").arg(info.fileName());
    return pages;
}

QByteArray loadPhoto(const QString &path, QString *error) {
    const QImage image = readImage(path, PhotoLongSide, error);
    if (image.isNull())
        return {};
    QByteArray jpeg;
    QBuffer buffer(&jpeg);
    buffer.open(QIODevice::WriteOnly);
    image.convertToFormat(QImage::Format_RGB32).save(&buffer, "JPEG", 85);
    return jpeg;
}
