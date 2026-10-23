#include "store.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace {
constexpr int SchemaVersion = 1;

QString tr(const char *text) { return QCoreApplication::translate("Store", text); }

QSqlDatabase database(const QString &connection) { return QSqlDatabase::database(connection, false); }

Item readItem(const QSqlQuery &query) {
    Item item;
    item.id = query.value("id").toInt();
    item.planId = query.value("plan_id").toInt();
    item.number = query.value("number").toInt();
    item.title = query.value("title").toString();
    item.trade = query.value("trade").toString();
    item.assignee = query.value("assignee").toString();
    item.notes = query.value("notes").toString();
    item.due = QDate::fromString(query.value("due").toString(), Qt::ISODate);
    item.status = query.value("status").toInt();
    item.x = query.value("x").toDouble();
    item.y = query.value("y").toDouble();
    item.created = QDateTime::fromString(query.value("created").toString(), Qt::ISODate);
    return item;
}

void bindItem(QSqlQuery &query, const Item &item) {
    query.bindValue(":plan_id", item.planId);
    query.bindValue(":number", item.number);
    query.bindValue(":title", item.title);
    query.bindValue(":trade", item.trade);
    query.bindValue(":assignee", item.assignee);
    query.bindValue(":notes", item.notes);
    query.bindValue(":due", item.due.isValid() ? item.due.toString(Qt::ISODate) : QString());
    query.bindValue(":status", item.status);
    query.bindValue(":x", item.x);
    query.bindValue(":y", item.y);
    query.bindValue(":created", item.created.toString(Qt::ISODate));
}
}

Store::Store() = default;

Store::~Store() { close(); }

bool Store::fail(const QString &message) const {
    m_error = message;
    return false;
}

bool Store::connect(const QString &path) {
    close();
    const QString connection = "corbel-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto db = QSqlDatabase::addDatabase("QSQLITE", connection);
    db.setDatabaseName(path);
    if (!db.open()) {
        const QString message = db.lastError().text();
        db = {};
        QSqlDatabase::removeDatabase(connection);
        return fail(tr("Could not open %1: %2").arg(path, message));
    }
    m_connection = connection;
    m_path = QFileInfo(path).absoluteFilePath();
    QSqlQuery(db).exec("PRAGMA foreign_keys = ON");
    return true;
}

void Store::close() {
    if (m_connection.isEmpty())
        return;
    {
        auto db = database(m_connection);
        db.close();
    }
    QSqlDatabase::removeDatabase(m_connection);
    m_connection.clear();
    m_path.clear();
}

bool Store::create(const QString &path, const QString &name) {
    if (QFileInfo::exists(path))
        return fail(tr("%1 already exists.").arg(path));
    if (!connect(path))
        return false;
    auto db = database(m_connection);
    db.transaction();
    QSqlQuery query(db);
    const QStringList schema = {
        "CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT)",
        "CREATE TABLE plans (id INTEGER PRIMARY KEY, name TEXT NOT NULL, width INTEGER, height INTEGER,"
        " image BLOB NOT NULL, position INTEGER NOT NULL)",
        "CREATE TABLE items (id INTEGER PRIMARY KEY, plan_id INTEGER NOT NULL REFERENCES plans(id),"
        " number INTEGER NOT NULL, title TEXT, trade TEXT, assignee TEXT, notes TEXT, due TEXT,"
        " status INTEGER NOT NULL DEFAULT 0, x REAL, y REAL, created TEXT)",
        "CREATE TABLE photos (id INTEGER PRIMARY KEY, item_id INTEGER NOT NULL REFERENCES items(id),"
        " name TEXT, data BLOB NOT NULL)",
        QString("PRAGMA user_version = %1").arg(SchemaVersion),
    };
    for (const auto &statement : schema)
        if (!query.exec(statement)) {
            const QString message = query.lastError().text();
            db.rollback();
            close();
            QFile::remove(path);
            return fail(tr("Could not create %1: %2").arg(path, message));
        }
    db.commit();
    return setName(name);
}

bool Store::open(const QString &path) {
    if (!QFileInfo(path).isFile())
        return fail(tr("%1 does not exist.").arg(path));
    if (!connect(path))
        return false;
    QSqlQuery query(database(m_connection));
    if (!query.exec("PRAGMA user_version") || !query.next() || query.value(0).toInt() < 1) {
        close();
        return fail(tr("%1 is not a Corbel project.").arg(path));
    }
    if (query.value(0).toInt() > SchemaVersion) {
        close();
        return fail(tr("%1 was made by a newer Corbel.").arg(path));
    }
    return true;
}

QString Store::name() const {
    QSqlQuery query(database(m_connection));
    query.exec("SELECT value FROM meta WHERE key = 'name'");
    return query.next() ? query.value(0).toString() : QFileInfo(m_path).completeBaseName();
}

bool Store::setName(const QString &name) {
    QSqlQuery query(database(m_connection));
    query.prepare("INSERT OR REPLACE INTO meta (key, value) VALUES ('name', :name)");
    query.bindValue(":name", name);
    return query.exec() || fail(query.lastError().text());
}

QList<Plan> Store::plans() const {
    QList<Plan> plans;
    QSqlQuery query(database(m_connection));
    query.exec("SELECT id, name, width, height FROM plans ORDER BY position, id");
    while (query.next())
        plans << Plan{query.value(0).toInt(), query.value(1).toString(), query.value(2).toInt(),
                      query.value(3).toInt()};
    return plans;
}

Plan Store::plan(int id) const {
    for (const auto &plan : plans())
        if (plan.id == id)
            return plan;
    return {};
}

int Store::addPlan(const QString &name, const QImage &image) {
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    if (image.isNull() || !image.save(&buffer, "PNG")) {
        fail(tr("The plan could not be stored."));
        return 0;
    }
    QSqlQuery query(database(m_connection));
    query.prepare("INSERT INTO plans (name, width, height, image, position)"
                  " VALUES (:name, :width, :height, :image, (SELECT IFNULL(MAX(position), 0) + 1 FROM plans))");
    query.bindValue(":name", name);
    query.bindValue(":width", image.width());
    query.bindValue(":height", image.height());
    query.bindValue(":image", png);
    if (!query.exec()) {
        fail(query.lastError().text());
        return 0;
    }
    return query.lastInsertId().toInt();
}

bool Store::renamePlan(int id, const QString &name) {
    QSqlQuery query(database(m_connection));
    query.prepare("UPDATE plans SET name = :name WHERE id = :id");
    query.bindValue(":name", name);
    query.bindValue(":id", id);
    return query.exec() || fail(query.lastError().text());
}

QImage Store::planImage(int id) const {
    QSqlQuery query(database(m_connection));
    query.prepare("SELECT image FROM plans WHERE id = :id");
    query.bindValue(":id", id);
    if (!query.exec() || !query.next())
        return {};
    return QImage::fromData(query.value(0).toByteArray(), "PNG");
}

QList<Item> Store::items() const {
    QList<Item> items;
    QSqlQuery query(database(m_connection));
    query.exec("SELECT * FROM items ORDER BY number");
    while (query.next())
        items << readItem(query);
    return items;
}

Item Store::item(int id) const {
    QSqlQuery query(database(m_connection));
    query.prepare("SELECT * FROM items WHERE id = :id");
    query.bindValue(":id", id);
    if (!query.exec() || !query.next())
        return {};
    return readItem(query);
}

int Store::nextNumber() const {
    QSqlQuery query(database(m_connection));
    query.exec("SELECT IFNULL(MAX(number), 0) + 1 FROM items");
    return query.next() ? query.value(0).toInt() : 1;
}

int Store::insertItem(const Item &item) {
    QSqlQuery query(database(m_connection));
    query.prepare(QString("INSERT INTO items (%1plan_id, number, title, trade, assignee, notes, due, status, x, y, created)"
                          " VALUES (%2:plan_id, :number, :title, :trade, :assignee, :notes, :due, :status, :x, :y, :created)")
                      .arg(item.id ? "id, " : "", item.id ? ":id, " : ""));
    if (item.id)
        query.bindValue(":id", item.id);
    bindItem(query, item);
    if (!query.exec()) {
        fail(query.lastError().text());
        return 0;
    }
    return item.id ? item.id : query.lastInsertId().toInt();
}

bool Store::updateItem(const Item &item) {
    QSqlQuery query(database(m_connection));
    query.prepare("UPDATE items SET plan_id = :plan_id, number = :number, title = :title, trade = :trade,"
                  " assignee = :assignee, notes = :notes, due = :due, status = :status, x = :x, y = :y,"
                  " created = :created WHERE id = :id");
    bindItem(query, item);
    query.bindValue(":id", item.id);
    return query.exec() || fail(query.lastError().text());
}

bool Store::removeItem(int id) {
    auto db = database(m_connection);
    db.transaction();
    if (!deleteItemRows(id)) {
        db.rollback();
        return false;
    }
    return db.commit();
}

// Without a transaction of its own, so restore() can use it inside one.
bool Store::deleteItemRows(int id) {
    QSqlQuery query(database(m_connection));
    query.prepare("DELETE FROM photos WHERE item_id = :id");
    query.bindValue(":id", id);
    bool ok = query.exec();
    query.prepare("DELETE FROM items WHERE id = :id");
    query.bindValue(":id", id);
    ok = ok && query.exec();
    return ok || fail(query.lastError().text());
}

QList<Photo> Store::photos(int itemId) const {
    QList<Photo> photos;
    QSqlQuery query(database(m_connection));
    query.prepare("SELECT id, name FROM photos WHERE item_id = :id ORDER BY id");
    query.bindValue(":id", itemId);
    query.exec();
    while (query.next())
        photos << Photo{query.value(0).toInt(), itemId, query.value(1).toString()};
    return photos;
}

QByteArray Store::photoData(int id) const {
    QSqlQuery query(database(m_connection));
    query.prepare("SELECT data FROM photos WHERE id = :id");
    query.bindValue(":id", id);
    return query.exec() && query.next() ? query.value(0).toByteArray() : QByteArray();
}

int Store::addPhoto(int itemId, const QString &name, const QByteArray &jpeg, int id) {
    QSqlQuery query(database(m_connection));
    query.prepare(QString("INSERT INTO photos (%1item_id, name, data) VALUES (%2:item_id, :name, :data)")
                      .arg(id ? "id, " : "", id ? ":id, " : ""));
    if (id)
        query.bindValue(":id", id);
    query.bindValue(":item_id", itemId);
    query.bindValue(":name", name);
    query.bindValue(":data", jpeg);
    if (!query.exec()) {
        fail(query.lastError().text());
        return 0;
    }
    return id ? id : query.lastInsertId().toInt();
}

bool Store::removePhoto(int id) {
    QSqlQuery query(database(m_connection));
    query.prepare("DELETE FROM photos WHERE id = :id");
    query.bindValue(":id", id);
    return query.exec() || fail(query.lastError().text());
}

QVariantMap Store::itemToMap(const Item &item) {
    return {{"id", item.id},         {"planId", item.planId},     {"number", item.number},
            {"title", item.title},   {"trade", item.trade},       {"assignee", item.assignee},
            {"notes", item.notes},   {"due", item.due},           {"status", item.status},
            {"x", item.x},           {"y", item.y},               {"created", item.created}};
}

Item Store::itemFromMap(const QVariantMap &map) {
    Item item;
    item.id = map.value("id").toInt();
    item.planId = map.value("planId").toInt();
    item.number = map.value("number").toInt();
    item.title = map.value("title").toString();
    item.trade = map.value("trade").toString();
    item.assignee = map.value("assignee").toString();
    item.notes = map.value("notes").toString();
    item.due = map.value("due").toDate();
    item.status = map.value("status").toInt();
    item.x = map.value("x").toDouble();
    item.y = map.value("y").toDouble();
    item.created = map.value("created").toDateTime();
    return item;
}

QVariantMap Store::snapshot(int id, bool withPhotos) const {
    const Item found = item(id);
    if (!found.id)
        return {};
    QVariantMap map = itemToMap(found);
    if (withPhotos) {
        QVariantList photos;
        for (const auto &photo : this->photos(id))
            photos << QVariantMap{{"id", photo.id}, {"name", photo.name}, {"data", photoData(photo.id)}};
        map.insert("photos", photos);
    }
    return map;
}

bool Store::restore(int id, const QVariantMap &snapshot) {
    auto db = database(m_connection);
    db.transaction();
    bool ok = true;
    if (snapshot.isEmpty()) {
        ok = deleteItemRows(id);
    } else {
        Item item = itemFromMap(snapshot);
        item.id = id;
        ok = this->item(id).id ? updateItem(item) : insertItem(item) == id;
        if (ok && snapshot.contains("photos")) {
            QSqlQuery query(db);
            query.prepare("DELETE FROM photos WHERE item_id = :id");
            query.bindValue(":id", id);
            ok = query.exec();
            for (const auto &value : snapshot.value("photos").toList()) {
                const auto photo = value.toMap();
                ok = ok && addPhoto(id, photo.value("name").toString(), photo.value("data").toByteArray(),
                                    photo.value("id").toInt());
            }
        }
    }
    if (!ok) {
        db.rollback();
        return false;
    }
    return db.commit();
}
