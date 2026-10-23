#include <QDir>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "fakepicker.h"
#include "store.h"
#include "theme.h"
#include "translator.h"

// The real window, offscreen, on a project with one floor and two items, in
// a language of the test's choosing.
struct Window {
    QTemporaryDir dir;
    Translator *translator = nullptr;
    Theme *theme = nullptr;
    FakePicker picker;
    Backend *backend = nullptr;
    QQmlApplicationEngine *engine = nullptr;
    QQuickWindow *window = nullptr;
    int first = 0, second = 0;

    explicit Window(const QByteArray &language = "en") {
        QFile file(dir.filePath("language"));
        file.open(QIODevice::WriteOnly);
        file.write(language);
        file.close();
        translator = new Translator(dir.filePath("language"));
        QCoreApplication::installTranslator(translator);
        theme = new Theme(dir.path());
        backend = new Backend(&picker);

        Store store;
        store.create(dir.filePath("site.corbel"), "Site");
        QImage plan(800, 500, QImage::Format_RGB32);
        plan.fill(Qt::white);
        const int planId = store.addPlan("Ground", plan);
        Item item;
        item.planId = planId;
        item.number = 1;
        item.title = "Cracked tile";
        item.x = .25;
        item.y = .5;
        first = store.insertItem(item);
        item.number = 2;
        item.title = "Loose socket";
        item.x = .75;
        second = store.insertItem(item);
        store.close();
        backend->openProjectAt(dir.filePath("site.corbel"));

        engine = new QQmlApplicationEngine;
        engine->addImageProvider("plan", new ProjectImageProvider(backend, false));
        engine->addImageProvider("photo", new ProjectImageProvider(backend, true));
        engine->rootContext()->setContextProperty("translator", translator);
        engine->rootContext()->setContextProperty("theme", theme);
        engine->rootContext()->setContextProperty("backend", backend);
        engine->load(QUrl("qrc:/Main.qml"));
        window = qobject_cast<QQuickWindow *>(engine->rootObjects().value(0));
        if (window) {
            window->resize(1400, 860);
            QTest::qWaitForWindowExposed(window);
            QTest::qWait(100);
        }
    }
    ~Window() {
        delete engine;
        delete backend;
        delete theme;
        QCoreApplication::removeTranslator(translator);
        delete translator;
    }

    // Delegates and popups sit in the visual item tree but not always under
    // the window as QObjects, so walk the items.
    static QQuickItem *search(QQuickItem *item, const QString &name) {
        if (item->objectName() == name)
            return item;
        for (auto *child : item->childItems())
            if (auto *found = search(child, name))
                return found;
        return nullptr;
    }
    QQuickItem *find(const QString &name) const { return search(window->contentItem(), name); }
    QPoint center(QQuickItem *item) const { return item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint(); }
    QPoint onPlan(double x, double y) const {
        auto *canvas = find("planCanvas");
        return canvas->mapToScene(QPointF(canvas->width() * x, canvas->height() * y)).toPoint();
    }
    // Lets a panel that just appeared finish its layout before it is clicked.
    void settle() { QTest::qWait(100); }
    void click(const QPoint &point) { QTest::mouseClick(window, Qt::LeftButton, {}, point); }
    void type(const QByteArray &text) {
        for (char c : text)
            QTest::keyClick(window, c);
    }
    void key(Qt::Key key, Qt::KeyboardModifiers modifiers = {}) { QTest::keyClick(window, key, modifiers); }
};

// Where a click lands, to within a pixel or so of the plan.
static bool near(double a, double b) { return qAbs(a - b) < .01; }

class WindowTests : public QObject {
    Q_OBJECT

private slots:
    void clickingThePlanPinsAnItemToName()
    {
        Window w;
        QVERIFY(w.window);
        w.click(w.onPlan(.5, .2));
        QCOMPARE(w.backend->totalCount(), 3);
        const int id = w.backend->selectedId();
        QVERIFY(near(w.backend->store().item(id).x, .5));
        QTRY_VERIFY(w.find("titleField")->hasActiveFocus());
        w.type("Gap at door");
        w.key(Qt::Key_Return);
        QCOMPARE(w.backend->store().item(id).title, QString("Gap at door"));
        // Enter is done typing, so single keys are shortcuts again.
        QVERIFY(!w.find("titleField")->hasActiveFocus());
        w.key(Qt::Key_3);
        QCOMPARE(w.backend->store().item(id).status, 2);
    }

    void switchingItemsWhileTypingKeepsEachTitle()
    {
        Window w;
        w.click(w.center(w.find("row1")));
        auto *title = w.find("titleField");
        QTRY_VERIFY(title->isVisible());
        w.settle();
        w.click(w.center(title));
        QVERIFY(title->hasActiveFocus());
        QTest::keyClick(w.window, Qt::Key_End);
        w.type(" by door");
        w.click(w.center(w.find("row2")));
        QCOMPARE(w.backend->store().item(w.first).title, QString("Cracked tile by door"));
        QCOMPARE(w.backend->store().item(w.second).title, QString("Loose socket"));
        QCOMPARE(title->property("text").toString(), QString("Loose socket"));
    }

    void draggingAPinMovesIt()
    {
        Window w;
        const QPoint from = w.onPlan(.25, .5), to = w.onPlan(.4, .3);
        QTest::mousePress(w.window, Qt::LeftButton, {}, from);
        for (int step = 1; step <= 10; ++step)
            QTest::mouseMove(w.window, from + (to - from) * step / 10);
        QTest::mouseRelease(w.window, Qt::LeftButton, {}, to);
        const Item moved = w.backend->store().item(w.first);
        QVERIFY(near(moved.x, .4));
        QVERIFY(near(moved.y, .3));
        QCOMPARE(w.backend->totalCount(), 2);
        w.key(Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(w.backend->store().item(w.first).x, .25);
    }

    void deleteRemovesAndUndoBringsBack()
    {
        Window w;
        w.click(w.onPlan(.75, .5));
        QCOMPARE(w.backend->selectedId(), w.second);
        w.key(Qt::Key_Delete);
        QCOMPARE(w.backend->totalCount(), 1);
        QVERIFY(!w.find("pin2"));
        w.key(Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(w.backend->totalCount(), 2);
        QTRY_VERIFY(w.find("pin2"));
        w.key(Qt::Key_Escape);
        QCOMPARE(w.backend->selectedId(), 0);
    }

    void theDatePickerSetsTheDueDate()
    {
        Window w;
        w.backend->select(w.first);
        QTRY_VERIFY(w.find("dueButton")->isVisible());
        w.settle();
        w.click(w.center(w.find("dueButton")));
        QQuickItem *today = nullptr;
        QTRY_VERIFY((today = w.find("dueToday")) && today->isVisible());
        w.click(w.center(today));
        QCOMPARE(w.backend->store().item(w.first).due, QDate::currentDate());
    }

    void rightToLeftLanguagesMirrorTheWindow()
    {
        Window w("he");
        // The plan sits on the right and the list on its left.
        QVERIFY(w.center(w.find("planView")).x() > w.center(w.find("itemList")).x());
        // Clicks still land where they are on the plan.
        w.click(w.onPlan(.6, .8));
        const Item added = w.backend->store().item(w.backend->selectedId());
        QVERIFY(near(added.x, .6));
        QVERIFY(near(added.y, .8));
    }

    void englishWindowKeepsThePlanOnTheLeft()
    {
        Window w;
        QVERIFY(w.center(w.find("planView")).x() < w.center(w.find("itemList")).x());
    }
};

int runWindowTests(int argc, char **argv)
{
    WindowTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "window_tests.moc"
