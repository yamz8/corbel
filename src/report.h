#pragma once

#include <QColor>
#include <QString>

#include "store.h"

// The colors a status prints in. Fixed rather than following the theme, so a
// report reads the same on paper whatever the desktop looked like.
QColor statusPrintColor(int status);
QString statusLabel(int status);

// What a filter picks out, in words: "Electrical · Open".
QString filterDescription(const Store &store, const ItemFilter &filter);

// Writes the closeout report: a summary and the item table, then each plan
// with its pins, then the photos. A filter narrows it to the items one trade
// or one person has to deal with, and the floors those items are on. The
// file appears only when it is complete.
bool writeReport(const Store &store, const QString &path, const ItemFilter &filter, QString *error);
