#pragma once

#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QImage>
#include <QList>
#include <QString>
#include <QVariantMap>

// The states a punch list item moves through, in order.
enum class Status { Open = 0, InProgress = 1, Done = 2 };

struct Plan {
    int id = 0;
    QString name;
    int width = 0;
    int height = 0;
};

struct Item {
    int id = 0;
    int planId = 0;
    int number = 0;
    QString title;
    QString trade;
    QString assignee;
    QString notes;
    QDate due;
    int status = 0;
    // Where the pin sits on its plan, as a fraction of the plan's width and
    // height, so it stays put whatever size the plan is drawn at.
    double x = 0;
    double y = 0;
    QDateTime created;
};

// Which items a list or report shows; empty fields match everything.
struct ItemFilter {
    QString trade;
    QString assignee;
    int floor = 0;
    int status = -1;

    bool isEmpty() const { return trade.isEmpty() && assignee.isEmpty() && !floor && status < 0; }
    bool matches(const Item &item) const;
    bool operator==(const ItemFilter &other) const {
        return trade == other.trade && assignee == other.assignee && floor == other.floor && status == other.status;
    }
};

struct Photo {
    int id = 0;
    int itemId = 0;
    QString name;
};

// One project is one SQLite file: its plans, items, and photos all live in
// it, so a project can be copied, mailed, or backed up as a single file and
// the plans and photos it was made from are never touched.
class Store {
public:
    Store();
    ~Store();
    Store(const Store &) = delete;
    Store &operator=(const Store &) = delete;

    bool create(const QString &path, const QString &name);
    bool open(const QString &path);
    void close();
    bool isOpen() const { return !m_connection.isEmpty(); }
    QString path() const { return m_path; }
    QString error() const { return m_error; }

    QString name() const;
    bool setName(const QString &name);

    QList<Plan> plans() const;
    Plan plan(int id) const;
    int addPlan(const QString &name, const QImage &image);
    bool renamePlan(int id, const QString &name);
    QImage planImage(int id) const;

    QList<Item> items() const;
    Item item(int id) const;
    int nextNumber() const;
    // Inserts the item, keeping its id when it has one (undo puts a deleted
    // item back exactly as it was).
    int insertItem(const Item &item);
    bool updateItem(const Item &item);
    bool removeItem(int id);

    QList<Photo> photos(int itemId) const;
    QByteArray photoData(int id) const;
    int addPhoto(int itemId, const QString &name, const QByteArray &jpeg, int id = 0);
    bool removePhoto(int id);

    // The whole state of an item, for undo. Photos are carried along only
    // when asked, since most edits leave them alone and they are large.
    QVariantMap snapshot(int id, bool withPhotos) const;
    // Makes the item match a snapshot; an empty snapshot of an id removes it.
    bool restore(int id, const QVariantMap &snapshot);

    // The same for a floor: its name and position, and with its contents
    // also the plan image and every item on it, so a deleted floor comes
    // back whole. An empty snapshot removes the floor and its items.
    QVariantMap planSnapshot(int id, bool withContents) const;
    bool restorePlan(int id, const QVariantMap &snapshot);
    bool removePlan(int id);

    static Item itemFromMap(const QVariantMap &map);
    static QVariantMap itemToMap(const Item &item);

private:
    bool connect(const QString &path);
    bool deleteItemRows(int id);
    bool restoreItemRows(int id, const QVariantMap &snapshot);
    bool fail(const QString &message) const;

    QString m_connection;
    QString m_path;
    mutable QString m_error;
};
