#pragma once

#include <QColor>
#include <QString>

class Store;

// The colors a status prints in. Fixed rather than following the theme, so a
// report reads the same on paper whatever the desktop looked like.
QColor statusPrintColor(int status);
QString statusLabel(int status);

// Writes the closeout report: a summary and the item table, then each plan
// with its pins, then the photos. The file appears only when it is complete.
bool writeReport(const Store &store, const QString &path, QString *error);
