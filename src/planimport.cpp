#include "planimport.h"

#include <QBuffer>
#include <QCollator>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

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
        return {{info.completeBaseName(), image.convertToFormat(QImage::Format_RGB32)}};
    }

    const QString pdftoppm = QStandardPaths::findExecutable("pdftoppm");
    if (pdftoppm.isEmpty()) {
        *error = tr("Opening PDF plans needs pdftoppm. Install it with: omarchy-pkg-add poppler");
        return {};
    }
    QTemporaryDir out;
    QProcess process;
    process.start(pdftoppm, {"-png", "-scale-to", QString::number(PlanLongSide), path, out.filePath("page")});
    if (!process.waitForFinished(120000) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        process.kill();
        const QString detail = QString::fromUtf8(process.readAllStandardError()).trimmed();
        *error = tr("%1 could not be rendered: %2").arg(info.fileName(), detail.isEmpty() ? process.errorString() : detail);
        return {};
    }

    // pdftoppm pads page numbers to the page count's width, so a natural sort
    // keeps page 10 after page 9.
    QStringList files = QDir(out.path()).entryList({"page*.png"}, QDir::Files);
    QCollator collator;
    collator.setNumericMode(true);
    std::sort(files.begin(), files.end(), collator);

    QList<PlanPage> pages;
    for (int i = 0; i < files.size(); ++i) {
        const QImage image = readImage(out.filePath(files[i]), PlanLongSide, error);
        if (image.isNull())
            return {};
        const QString name = files.size() == 1 ? info.completeBaseName()
                                               : tr("%1, page %2").arg(info.completeBaseName()).arg(i + 1);
        pages << PlanPage{name, image.convertToFormat(QImage::Format_RGB32)};
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
