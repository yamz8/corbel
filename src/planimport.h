#pragma once

#include <QByteArray>
#include <QList>
#include <QSize>
#include <QString>

// A floor ready to store: its picture as PNG, and the picture's size.
struct PlanPage {
    QString name;
    QByteArray png;
    QSize size;
};

// Turns a floor plan file into pictures: every page of a PDF (through
// poppler's pdftoppm, whose PNGs are kept as they are), or a single image.
// The file itself is only read.
QList<PlanPage> loadPlan(const QString &path, QString *error);

// Photos are re-encoded as JPEG no larger than this on their long side, so a
// project holding a few hundred of them stays a manageable file.
QByteArray loadPhoto(const QString &path, QString *error);
