#include <QDir>
#include <QFile>
#include <QPainter>
#include <QPdfWriter>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "filepicker.h"
#include "planimport.h"
#include "report.h"
#include "store.h"
#include "theme.h"
#include "translator.h"

// Answers every dialog with the files it was given, as the portal would.
class FakePicker : public FilePicker {
public:
    QStringList answers;
    QString lastPurpose;
    void open(const QString &purpose, const QString &, const QList<FileFilter> &, bool, const QString &) override {
        lastPurpose = purpose;
        QList<QUrl> urls;
        for (const auto &answer : answers)
            urls << QUrl::fromLocalFile(answer);
        emit selected(purpose, urls);
    }
    void save(const QString &purpose, const QString &, const QString &, const FileFilter &) override {
        lastPurpose = purpose;
        emit selected(purpose, {QUrl::fromLocalFile(answers.value(0))});
    }
};

class CorbelTests : public QObject {
    Q_OBJECT

private:
    static QString writeImage(const QString &path, const QSize &size, const QColor &color) {
        QImage image(size, QImage::Format_RGB32);
        image.fill(color);
        image.save(path);
        return path;
    }
    // A project with one plan, ready for items.
    static QString makeProject(const QTemporaryDir &dir) {
        Store store;
        const QString path = dir.filePath("site.corbel");
        if (!store.create(path, "Site"))
            return {};
        QImage plan(800, 600, QImage::Format_RGB32);
        plan.fill(Qt::white);
        store.addPlan("Ground", plan);
        return path;
    }

private slots:
    // Settings (the last project, the last folder) go to a test location,
    // not the user's own.
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void storeCreatesAndReopens()
    {
        QTemporaryDir dir;
        const QString path = makeProject(dir);
        QVERIFY(!path.isEmpty());
        Store store;
        QVERIFY(store.open(path));
        QCOMPARE(store.name(), QString("Site"));
        QCOMPARE(store.plans().size(), 1);
        QCOMPARE(store.planImage(store.plans().first().id).size(), QSize(800, 600));
    }

    void storeRefusesWhatIsNotAProject()
    {
        QTemporaryDir dir;
        QFile file(dir.filePath("notes.corbel"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("hello");
        file.close();
        Store store;
        QVERIFY(!store.open(file.fileName()));
        QVERIFY(!store.isOpen());
        QVERIFY(!store.create(file.fileName(), "x"));
        QCOMPARE(QFileInfo(file.fileName()).size(), 5);
    }

    void storeKeepsItemsAndPhotos()
    {
        QTemporaryDir dir;
        Store store;
        QVERIFY(store.open(makeProject(dir)));
        Item item;
        item.planId = store.plans().first().id;
        item.number = store.nextNumber();
        item.title = "Cracked tile";
        item.due = QDate(2026, 11, 30);
        item.x = .25;
        item.y = .75;
        const int id = store.insertItem(item);
        QVERIFY(id);
        QCOMPARE(store.nextNumber(), 2);
        QVERIFY(store.addPhoto(id, "a.jpg", "jpegdata"));

        const QVariantMap snapshot = store.snapshot(id, true);
        QVERIFY(store.removeItem(id));
        QVERIFY(store.items().isEmpty());
        QVERIFY(store.restore(id, snapshot));
        const Item back = store.item(id);
        QCOMPARE(back.title, QString("Cracked tile"));
        QCOMPARE(back.due, QDate(2026, 11, 30));
        QCOMPARE(back.x, .25);
        QCOMPARE(store.photos(id).size(), 1);
        QCOMPARE(store.photoData(store.photos(id).first().id), QByteArray("jpegdata"));
        QVERIFY(store.restore(id, {}));
        QVERIFY(!store.item(id).id);
    }

    void importsImagesWithoutTouchingThem()
    {
        QTemporaryDir dir;
        const QString path = writeImage(dir.filePath("plan.png"), QSize(6000, 3000), Qt::white);
        const auto modified = QFileInfo(path).lastModified();
        QString error;
        const auto pages = loadPlan(path, &error);
        QCOMPARE(pages.size(), 1);
        QCOMPARE(pages.first().name, QString("plan"));
        // Large plans are scaled to a 4000 pixel long side.
        QCOMPARE(pages.first().image.size(), QSize(4000, 2000));
        QCOMPARE(QFileInfo(path).lastModified(), modified);
        QVERIFY(loadPlan(dir.filePath("missing.pdf"), &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void importsEveryPageOfAPdf()
    {
        if (QStandardPaths::findExecutable("pdftoppm").isEmpty())
            QSKIP("pdftoppm is not installed");
        QTemporaryDir dir;
        const QString path = dir.filePath("plans.pdf");
        {
            QPdfWriter writer(path);
            QPainter painter(&writer);
            painter.drawText(100, 100, "Ground");
            writer.newPage();
            painter.drawText(100, 100, "First");
        }
        QString error;
        const auto pages = loadPlan(path, &error);
        QCOMPARE(pages.size(), 2);
        QVERIFY(pages[1].name.contains("2"));
        QVERIFY(!pages[0].image.isNull());
    }

    void backendAddsEditsAndUndoes()
    {
        QTemporaryDir dir;
        FakePicker picker;
        Backend backend(&picker);
        QVERIFY(backend.openProjectAt(makeProject(dir)));
        QVERIFY(backend.currentPlan() > 0);

        QSignalSpy added(&backend, &Backend::itemAdded);
        const int id = backend.addItem(.5, .4);
        QVERIFY(id);
        QCOMPARE(added.size(), 1);
        QCOMPARE(backend.selectedId(), id);
        backend.setField(id, "title", "  Door sticks ");
        backend.setField(id, "status", 1);
        backend.setField(id, "due", QDate(2026, 12, 1));
        QCOMPARE(backend.selected().value("title").toString(), QString("Door sticks"));
        QCOMPARE(backend.statusCounts().value(1).toInt(), 1);

        backend.undo();
        QVERIFY(!backend.selected().value("due").toDate().isValid());
        backend.undo();
        QCOMPARE(backend.selected().value("status").toInt(), 0);
        backend.redo();
        QCOMPARE(backend.selected().value("status").toInt(), 1);

        backend.removeItem(id);
        QCOMPARE(backend.totalCount(), 0);
        backend.undo();
        QCOMPARE(backend.totalCount(), 1);
        QCOMPARE(backend.selected().value("title").toString(), QString("Door sticks"));
    }

    void unchangedFieldsLeaveNoUndo()
    {
        QTemporaryDir dir;
        Backend backend(nullptr);
        QVERIFY(backend.openProjectAt(makeProject(dir)));
        const int id = backend.addItem(.1, .1);
        backend.setField(id, "title", "Same");
        QSignalSpy history(&backend, &Backend::historyChanged);
        backend.setField(id, "title", "Same");
        QCOMPARE(history.size(), 0);
    }

    void photosAreAddedAndUndone()
    {
        QTemporaryDir dir;
        FakePicker picker;
        Backend backend(&picker);
        QVERIFY(backend.openProjectAt(makeProject(dir)));
        const int id = backend.addItem(.3, .3);
        picker.answers = {writeImage(dir.filePath("a.jpg"), QSize(3000, 1500), Qt::red),
                          writeImage(dir.filePath("b.png"), QSize(100, 100), Qt::blue)};
        backend.addPhotos();
        QCOMPARE(picker.lastPurpose, QString("photos"));
        QCOMPARE(backend.selected().value("photos").toList().size(), 2);
        // Photos are kept no larger than 2000 pixels on their long side.
        const int photoId = backend.selected().value("photos").toList().first().toMap().value("id").toInt();
        QCOMPARE(backend.photoImage(photoId, {}).size(), QSize(2000, 1000));
        backend.removePhoto(photoId);
        QCOMPARE(backend.selected().value("photos").toList().size(), 1);
        backend.undo();
        QCOMPARE(backend.selected().value("photos").toList().size(), 2);
        QCOMPARE(backend.store().item(id).id, id);
    }

    void filtersKeepTheSelectedItem()
    {
        QTemporaryDir dir;
        Backend backend(nullptr);
        QVERIFY(backend.openProjectAt(makeProject(dir)));
        const int painting = backend.addItem(.2, .2);
        backend.setField(painting, "trade", "Painting");
        const int electrical = backend.addItem(.4, .4);
        backend.setField(electrical, "trade", "Electrical");
        backend.select(0);
        backend.setTradeFilter("Painting");
        QCOMPARE(backend.items()->rowCount(), 1);
        QCOMPARE(backend.pins()->rowCount(), 1);
        // A new item takes the filter's trade, so it stays in the list.
        const int added = backend.addItem(.6, .6);
        QCOMPARE(backend.store().item(added).trade, QString("Painting"));
        QCOMPARE(backend.items()->rowCount(), 2);
        backend.select(electrical);
        QCOMPARE(backend.items()->rowCount(), 3);
        backend.clearFilters();
        QVERIFY(!backend.filtered());
        QCOMPARE(backend.usedTrades(), QStringList({"Electrical", "Painting"}));
    }

    void anOpenProjectSurvivesABadOpen()
    {
        QTemporaryDir dir;
        Backend backend(nullptr);
        const QString path = makeProject(dir);
        QVERIFY(backend.openProjectAt(path));
        QVERIFY(!backend.openProjectAt(dir.filePath("missing.corbel")));
        QVERIFY(!backend.error().isEmpty());
        QCOMPARE(backend.projectPath(), QFileInfo(path).absoluteFilePath());
    }

    void reportIsWrittenWhole()
    {
        QTemporaryDir dir;
        Backend backend(nullptr);
        QVERIFY(backend.openProjectAt(makeProject(dir)));
        for (int i = 0; i < 60; ++i)
            backend.setField(backend.addItem(i / 60.0, .5), "title", QString("Item %1").arg(i));
        const QString target = dir.filePath("report");
        QVERIFY(backend.exportReportTo(target));
        QFile pdf(target + ".pdf");
        QVERIFY(pdf.open(QIODevice::ReadOnly));
        QVERIFY(pdf.read(5) == "%PDF-");
        // Nothing but the project, its report, and nothing half-written.
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files).size(), 2);

        QString error;
        QVERIFY(!writeReport(backend.store(), dir.filePath("missing/dir/report.pdf"), &error));
        QVERIFY(!error.isEmpty());
    }

    void statusesHaveNamesAndPrintColors()
    {
        QCOMPARE(statusLabel(0), QString("Open"));
        QCOMPARE(statusLabel(2), QString("Done"));
        QVERIFY(statusPrintColor(0) != statusPrintColor(2));
    }

    void themeReadsStatusColors()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        QFile colors(dir.path() + "/theme/colors.toml");
        QVERIFY(colors.open(QIODevice::WriteOnly));
        colors.write("accent = \"#112233\"\nred = \"#aa0000\"\ngreen = \"#00aa00\"\n");
        colors.close();
        Theme theme(dir.path());
        QCOMPARE(theme.open(), QString("#aa0000"));
        QCOMPARE(theme.done(), QString("#00aa00"));
        QCOMPARE(theme.progress(), QString("#F5A524"));
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        QFile colors(dir.path() + "/theme/colors.toml");
        QVERIFY(colors.open(QIODevice::WriteOnly));
        colors.write("accent = \"#112233\"\n");
        colors.close();

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        auto write = [](const QString &path, const QByteArray &data) {
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
        };
        QVERIFY(write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n"));
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QVERIFY(write(response, "{\"int\":0}"));

        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        QVERIFY(write(response, "{\"int\":6}"));
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        write(dir.path() + "/pt.json", R"({"Save": "Guardar", "Open": "Abrir"})");
        write(dir.path() + "/pt-BR.json", R"({"Save": "Salvar"})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("pt-BR"));
        QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void translatorPicksPluralForms()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "ru\n");
        write(dir.path() + "/en.json", R"({"%n clips": {"one": "%n clip", "other": "%n clips"}})");
        write(dir.path() + "/ru.json", R"({"%n clips": {"one": "%n клип", "few": "%n клипа", "many": "%n клипов", "other": "%n клипа"}})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 21), QString("%n клип"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 22), QString("%n клипа"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 25), QString("%n клипов"));
        QCOMPARE(Translator::pluralCategory("ar", 11), QString("many"));
        QCOMPARE(Translator::pluralCategory("ja", 1), QString("other"));
    }

private:
    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(CorbelTests)
#include "corbel_tests.moc"
