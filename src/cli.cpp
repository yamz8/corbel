#include "cli.h"

#include "planimport.h"
#include "report.h"
#include "store.h"

#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <cstdio>

namespace {
const QStringList Commands = {"new", "import", "plans", "items", "add", "set", "export", "skill", "help"};

void print(FILE *stream, const QString &text) { fprintf(stream, "%s\n", qPrintable(text)); }
void print(const QJsonObject &object) { fprintf(stdout, "%s", QJsonDocument(object).toJson().constData()); }
int fail(const QString &message) {
    print(stderr, message);
    return 1;
}

const char *StatusNames[] = {"open", "progress", "done"};

// open, progress (or in-progress), done; -1 when it is none of them.
int parseStatus(const QString &value) {
    const QString name = value.toLower();
    if (name == "in-progress" || name == "inprogress")
        return 1;
    for (int i = 0; i < 3; ++i)
        if (name == StatusNames[i] || name == QString::number(i))
            return i;
    return -1;
}

QJsonObject toJson(const Store &store, const Item &item) {
    return {{"number", item.number},
            {"title", item.title},
            {"floor", item.planId},
            {"floorName", store.plan(item.planId).name},
            {"trade", item.trade},
            {"assignee", item.assignee},
            {"due", item.due.isValid() ? QJsonValue(item.due.toString(Qt::ISODate)) : QJsonValue()},
            {"status", StatusNames[qBound(0, item.status, 2)]},
            {"notes", item.notes},
            {"x", item.x},
            {"y", item.y},
            {"photos", int(store.photos(item.id).size())}};
}

struct Command {
    QCommandLineParser parser;
    Command(const QString &name, const QString &description, const QString &extra = {}) {
        parser.setApplicationDescription(description);
        parser.addHelpOption();
        parser.addPositionalArgument(name, description, name);
        parser.addPositionalArgument("project", "Corbel project file", "<project.corbel>");
        if (!extra.isEmpty())
            parser.addPositionalArgument(extra, extra, "<" + extra + ">");
        parser.addOption({"json", "Print the result as JSON"});
    }
    void option(const QString &name, const QString &description, const QString &value = {}) {
        parser.addOption(value.isEmpty() ? QCommandLineOption(name, description)
                                         : QCommandLineOption(name, description, value));
    }
    bool json() const { return parser.isSet("json"); }
    QString argument(int index) const { return parser.positionalArguments().value(index); }
    bool open(Store &store) const {
        if (argument(1).isEmpty()) {
            print(stderr, "Name a Corbel project.");
            return false;
        }
        if (!store.open(argument(1))) {
            print(stderr, store.error());
            return false;
        }
        return true;
    }
    // Item options shared by add and set; false (with a message) on a bad value.
    void itemOptions() {
        option("title", "What is wrong", "text");
        option("trade", "Trade responsible, e.g. Electrical", "trade");
        option("assignee", "Person or company to fix it", "name");
        option("due", "Due date, YYYY-MM-DD (empty clears it)", "date");
        option("status", "open, progress, or done", "status");
        option("notes", "Longer description", "text");
        option("floor", "Floor (plan) id from corbel plans", "id");
        option("x", "Pin position across the plan, 0 to 1", "fraction");
        option("y", "Pin position down the plan, 0 to 1", "fraction");
    }
    bool applyOptions(const Store &store, Item &item) const {
        if (parser.isSet("title")) item.title = parser.value("title").trimmed();
        if (parser.isSet("trade")) item.trade = parser.value("trade").trimmed();
        if (parser.isSet("assignee")) item.assignee = parser.value("assignee").trimmed();
        if (parser.isSet("notes")) item.notes = parser.value("notes");
        if (parser.isSet("due")) {
            item.due = QDate::fromString(parser.value("due"), Qt::ISODate);
            if (!parser.value("due").isEmpty() && !item.due.isValid())
                return !fail("--due takes a date as YYYY-MM-DD.");
        }
        if (parser.isSet("status")) {
            item.status = parseStatus(parser.value("status"));
            if (item.status < 0)
                return !fail("--status is open, progress, or done.");
        }
        if (parser.isSet("floor")) {
            item.planId = parser.value("floor").toInt();
            if (!store.plan(item.planId).id)
                return !fail(QString("There is no floor %1; corbel plans lists them.").arg(parser.value("floor")));
        }
        for (const char *axis : {"x", "y"})
            if (parser.isSet(axis)) {
                bool ok = false;
                const double value = parser.value(axis).toDouble(&ok);
                if (!ok || value < 0 || value > 1)
                    return !fail(QString("--%1 is a fraction from 0 to 1.").arg(axis));
                (QString(axis) == "x" ? item.x : item.y) = value;
            }
        return true;
    }
};

int addPlans(Store &store, const QString &file, QJsonArray *added) {
    QString error;
    const auto pages = loadPlan(file, &error);
    if (pages.isEmpty())
        return fail(error);
    for (const auto &page : pages) {
        const int id = store.addPlan(page.name, page.image);
        if (!id)
            return fail(store.error());
        *added << QJsonObject{{"id", id}, {"name", page.name}};
    }
    return 0;
}

int newProject(const QStringList &arguments) {
    Command command("new", "Create a project, optionally starting with a floor plan.");
    command.option("name", "Project name (default: the file name)", "name");
    command.option("plan", "Floor plan to import, a PDF or image", "file");
    command.parser.process(arguments);
    QString path = command.argument(1);
    if (path.isEmpty())
        return fail("Name the project file to create.");
    if (!path.endsWith(".corbel"))
        path += ".corbel";
    Store store;
    const QString name = command.parser.value("name").isEmpty() ? QFileInfo(path).completeBaseName()
                                                                 : command.parser.value("name");
    if (!store.create(path, name))
        return fail(store.error());
    QJsonArray plans;
    if (command.parser.isSet("plan") && addPlans(store, command.parser.value("plan"), &plans)) {
        store.close();
        QFile::remove(path);
        return 1;
    }
    if (command.json())
        print({{"ok", true}, {"project", store.path()}, {"name", name}, {"plans", plans}});
    else
        print(stdout, QString("Created %1").arg(store.path()));
    return 0;
}

int importPlan(const QStringList &arguments) {
    Command command("import", "Add a floor plan to a project; each PDF page becomes a floor.", "plan");
    command.parser.process(arguments);
    Store store;
    if (!command.open(store))
        return 1;
    if (command.argument(2).isEmpty())
        return fail("Name the floor plan to import.");
    QJsonArray plans;
    if (addPlans(store, command.argument(2), &plans))
        return 1;
    if (command.json())
        print({{"ok", true}, {"plans", plans}});
    else
        for (const auto &plan : plans)
            print(stdout, QString("%1  %2").arg(plan.toObject().value("id").toInt()).arg(plan.toObject().value("name").toString()));
    return 0;
}

int plans(const QStringList &arguments) {
    Command command("plans", "List the floors (plans) in a project with their ids.");
    command.parser.process(arguments);
    Store store;
    if (!command.open(store))
        return 1;
    const auto items = store.items();
    QJsonArray list;
    for (const auto &plan : store.plans()) {
        int count = 0;
        for (const auto &item : items)
            count += item.planId == plan.id;
        list << QJsonObject{{"id", plan.id}, {"name", plan.name}, {"width", plan.width}, {"height", plan.height}, {"items", count}};
        if (!command.json())
            print(stdout, QString("%1  %2  (%3 items)").arg(plan.id).arg(plan.name).arg(count));
    }
    if (command.json())
        print({{"ok", true}, {"plans", list}});
    return 0;
}

int items(const QStringList &arguments) {
    Command command("items", "List the punch list, optionally filtered.");
    command.option("status", "Only items with this status: open, progress, done", "status");
    command.option("trade", "Only items for this trade", "trade");
    command.option("assignee", "Only items assigned to this person", "name");
    command.option("floor", "Only items on this floor id", "id");
    command.parser.process(arguments);
    Store store;
    if (!command.open(store))
        return 1;
    const int status = command.parser.isSet("status") ? parseStatus(command.parser.value("status")) : -1;
    if (command.parser.isSet("status") && status < 0)
        return fail("--status is open, progress, or done.");
    QJsonArray list;
    for (const auto &item : store.items()) {
        if ((status >= 0 && item.status != status) ||
            (command.parser.isSet("trade") && item.trade != command.parser.value("trade")) ||
            (command.parser.isSet("assignee") && item.assignee != command.parser.value("assignee")) ||
            (command.parser.isSet("floor") && item.planId != command.parser.value("floor").toInt()))
            continue;
        list << toJson(store, item);
        if (!command.json())
            print(stdout, QString("#%1  [%2]  %3  — %4, %5%6")
                              .arg(item.number)
                              .arg(StatusNames[qBound(0, item.status, 2)])
                              .arg(item.title.isEmpty() ? "(untitled)" : item.title)
                              .arg(item.trade.isEmpty() ? "no trade" : item.trade)
                              .arg(item.assignee.isEmpty() ? "unassigned" : item.assignee)
                              .arg(item.due.isValid() ? ", due " + item.due.toString(Qt::ISODate) : QString()));
    }
    if (command.json())
        print({{"ok", true}, {"items", list}});
    return 0;
}

int add(const QStringList &arguments) {
    Command command("add", "Add an item and pin it to a floor.");
    command.itemOptions();
    command.parser.process(arguments);
    Store store;
    if (!command.open(store))
        return 1;
    const auto plans = store.plans();
    if (plans.isEmpty())
        return fail("The project has no floor plan yet; corbel import adds one.");
    Item item;
    item.planId = plans.first().id;
    item.x = item.y = .5;
    item.number = store.nextNumber();
    item.created = QDateTime::currentDateTime();
    if (!command.applyOptions(store, item))
        return 1;
    if (item.title.isEmpty())
        return fail("Give the item a --title.");
    item.id = store.insertItem(item);
    if (!item.id)
        return fail(store.error());
    if (command.json())
        print({{"ok", true}, {"item", toJson(store, item)}});
    else
        print(stdout, QString("Added #%1").arg(item.number));
    return 0;
}

int set(const QStringList &arguments) {
    Command command("set", "Change an item, found by its number.", "number");
    command.itemOptions();
    command.parser.process(arguments);
    Store store;
    if (!command.open(store))
        return 1;
    Item item;
    for (const auto &candidate : store.items())
        if (QString::number(candidate.number) == command.argument(2).remove('#'))
            item = candidate;
    if (!item.id)
        return fail(QString("There is no item %1.").arg(command.argument(2)));
    if (!command.applyOptions(store, item))
        return 1;
    if (!store.updateItem(item))
        return fail(store.error());
    if (command.json())
        print({{"ok", true}, {"item", toJson(store, item)}});
    else
        print(stdout, QString("Updated #%1").arg(item.number));
    return 0;
}

int exportReport(const QStringList &arguments) {
    Command command("export", "Write the closeout report as a PDF.", "report.pdf");
    command.parser.process(arguments);
    Store store;
    if (!command.open(store))
        return 1;
    const QString target = command.argument(2);
    if (target.isEmpty())
        return fail("Name the PDF to write.");
    QString error;
    if (!writeReport(store, target, &error))
        return fail(error);
    if (command.json())
        print({{"ok", true}, {"report", QFileInfo(target).absoluteFilePath()}});
    else
        print(stdout, QString("Wrote %1").arg(QFileInfo(target).absoluteFilePath()));
    return 0;
}

int skill(const QStringList &arguments) {
    if (arguments.value(2) != "install")
        return fail("Usage: corbel skill install");
    const QString directory = QDir::homePath() + "/.claude/skills/corbel";
    QFile source(":/skill.md");
    if (!QDir().mkpath(directory) || !source.open(QIODevice::ReadOnly))
        return fail("Could not install the skill to " + directory);
    QSaveFile target(directory + "/SKILL.md");
    if (!target.open(QIODevice::WriteOnly) || target.write(source.readAll()) < 0 || !target.commit())
        return fail("Could not write " + target.fileName());
    print(stdout, "Installed " + target.fileName());
    return 0;
}
}

bool isCliCommand(const char *argument) { return Commands.contains(QString::fromUtf8(argument)); }

QString cliSummary() {
    return "Commands:\n"
           "  open [project]                 Open the app\n"
           "  new <project> [--plan file]    Create a project\n"
           "  import <project> <plan>        Add a floor plan (PDF or image)\n"
           "  plans <project>                List floors\n"
           "  items <project> [filters]      List the punch list\n"
           "  add <project> --title ...      Add an item\n"
           "  set <project> <number> ...     Change an item\n"
           "  export <project> <report.pdf>  Write the closeout report\n"
           "  skill install                  Install the agent skill\n\n"
           "Every command takes --help, and --json for machine-readable output.";
}

int runCli(const QStringList &arguments) {
    const QString name = arguments.value(1);
    if (name.isEmpty() || name == "help") {
        print(stdout, "Corbel — punch lists on floor plans.\n\n" + cliSummary());
        return 0;
    }
    if (name == "new") return newProject(arguments);
    if (name == "import") return importPlan(arguments);
    if (name == "plans") return plans(arguments);
    if (name == "items") return items(arguments);
    if (name == "add") return add(arguments);
    if (name == "set") return set(arguments);
    if (name == "export") return exportReport(arguments);
    if (name == "skill") return skill(arguments);
    return fail("Unknown command " + name);
}
