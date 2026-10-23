#pragma once

#include <QStringList>

// Commands that work on a project file without a window, for scripts and
// agents: corbel new, import, plans, items, add, set, export, skill install.
bool isCliCommand(const char *argument);
QString cliSummary();
int runCli(const QStringList &arguments);
