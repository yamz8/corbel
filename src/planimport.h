#pragma once

#include <QImage>
#include <QList>
#include <QString>

struct PlanPage {
    QString name;
    QImage image;
};

// Turns a floor plan file into images: every page of a PDF (through
// poppler's pdftoppm), or a single image. The file itself is only read.
QList<PlanPage> loadPlan(const QString &path, QString *error);

// Photos are re-encoded as JPEG no larger than this on their long side, so a
// project holding a few hundred of them stays a manageable file.
QByteArray loadPhoto(const QString &path, QString *error);
