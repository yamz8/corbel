#include "backend.h"

#include "filepicker.h"
#include "planimport.h"
#include "report.h"

#include <QCollator>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace {
// Trades every punch list starts with; anything typed in is added beside them.
QStringList defaultTrades() {
    return {QObject::tr("General"),   QObject::tr("Electrical"), QObject::tr("Plumbing"), QObject::tr("HVAC"),
            QObject::tr("Drywall"),   QObject::tr("Painting"),   QObject::tr("Flooring"), QObject::tr("Carpentry"),
            QObject::tr("Doors"),     QObject::tr("Windows"),    QObject::tr("Roofing"),  QObject::tr("Concrete"),
            QObject::tr("Tiling"),    QObject::tr("Fire safety"), QObject::tr("Landscaping"), QObject::tr("Cleaning")};
}

QStringList sorted(QStringList list) {
    list.removeAll(QString());
    list.removeDuplicates();
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(list.begin(), list.end(), collator);
    return list;
}

FileFilter projectFilter() { return {QObject::tr("Corbel projects"), {"*.corbel"}, {}}; }
}

Backend::Backend(FilePicker *picker, QObject *parent) : QObject(parent), m_picker(picker) {
    if (m_picker) {
        connect(m_picker, &FilePicker::selected, this, &Backend::pickerSelected);
        connect(m_picker, &FilePicker::failed, this, &Backend::setError);
    }
}

Backend::~Backend() = default;

QVariantList Backend::plans() const {
    if (!isOpen())
        return {};
    QVariantList list;
    for (const auto &plan : m_store.plans()) {
        int count = 0;
        for (const auto &item : m_all)
            count += item.planId == plan.id;
        list << QVariantMap{{"id", plan.id}, {"name", plan.name}, {"width", plan.width},
                            {"height", plan.height}, {"count", count}};
    }
    return list;
}

void Backend::setCurrentPlan(int id) {
    if (id == m_currentPlan)
        return;
    m_currentPlan = id;
    emit currentPlanChanged();
    refresh();
}

QSize Backend::currentPlanSize() const {
    if (!isOpen())
        return {};
    const Plan plan = m_store.plan(m_currentPlan);
    return QSize(plan.width, plan.height);
}

QVariantMap Backend::selected() const {
    if (!isOpen())
        return {};
    for (const auto &item : m_all)
        if (item.id == m_selectedId) {
            QVariantMap map = Store::itemToMap(item);
            map.insert("planName", m_store.plan(item.planId).name);
            QVariantList photos;
            for (const auto &photo : m_store.photos(item.id))
                photos << QVariantMap{{"id", photo.id}, {"name", photo.name}};
            map.insert("photos", photos);
            return map;
        }
    return {};
}

QStringList Backend::trades() const { return sorted(defaultTrades() + usedTrades()); }

QStringList Backend::usedTrades() const {
    QStringList list;
    for (const auto &item : m_all)
        list << item.trade;
    return sorted(list);
}

QStringList Backend::assignees() const {
    QStringList list;
    for (const auto &item : m_all)
        list << item.assignee;
    return sorted(list);
}

QVariantList Backend::statusCounts() const {
    int counts[3] = {0, 0, 0};
    for (const auto &item : m_all)
        counts[qBound(0, item.status, 2)]++;
    return {counts[0], counts[1], counts[2]};
}

void Backend::setTradeFilter(const QString &trade) {
    if (trade == m_tradeFilter)
        return;
    m_tradeFilter = trade;
    emit filtersChanged();
    refresh();
}

void Backend::setAssigneeFilter(const QString &assignee) {
    if (assignee == m_assigneeFilter)
        return;
    m_assigneeFilter = assignee;
    emit filtersChanged();
    refresh();
}

void Backend::setFloorFilter(int planId) {
    if (planId == m_floorFilter)
        return;
    m_floorFilter = planId;
    emit filtersChanged();
    if (planId)
        setCurrentPlan(planId);
    refresh();
}

void Backend::setStatusFilter(int status) {
    if (status == m_statusFilter)
        return;
    m_statusFilter = status;
    emit filtersChanged();
    refresh();
}

bool Backend::filtered() const {
    return !m_tradeFilter.isEmpty() || !m_assigneeFilter.isEmpty() || m_floorFilter || m_statusFilter >= 0;
}

void Backend::clearFilters() {
    m_tradeFilter.clear();
    m_assigneeFilter.clear();
    m_floorFilter = 0;
    m_statusFilter = -1;
    emit filtersChanged();
    refresh();
}

// The selected item always shows, so a new item doesn't vanish into a filter
// before it has been given the trade or status the filter is looking for.
bool Backend::matches(const Item &item, bool ignoreFloor) const {
    if (item.id == m_selectedId)
        return true;
    return (m_tradeFilter.isEmpty() || item.trade == m_tradeFilter) &&
           (m_assigneeFilter.isEmpty() || item.assignee == m_assigneeFilter) &&
           (ignoreFloor || !m_floorFilter || item.planId == m_floorFilter) &&
           (m_statusFilter < 0 || item.status == m_statusFilter);
}

void Backend::refresh() {
    m_all = m_store.isOpen() ? m_store.items() : QList<Item>();
    m_photoCounts.clear();
    for (const auto &item : m_all)
        m_photoCounts.insert(item.id, m_store.photos(item.id).size());
    QHash<int, QString> names;
    for (const auto &plan : m_store.plans())
        names.insert(plan.id, plan.name);

    QList<Item> listed, pinned;
    for (const auto &item : m_all) {
        if (matches(item, false))
            listed << item;
        if (item.planId == m_currentPlan && matches(item, true))
            pinned << item;
    }
    m_items.setItems(listed, names, m_photoCounts);
    m_pins.setItems(pinned, names, m_photoCounts);
    emit itemsChanged();
    emit plansChanged();
    emit selectionChanged();
}

QImage Backend::planImage(int id) {
    if (!m_planCache.contains(id))
        m_planCache.insert(id, m_store.planImage(id));
    return m_planCache.value(id);
}

QImage Backend::photoImage(int id, const QSize &requested) {
    QImage image = QImage::fromData(m_store.photoData(id));
    if (!image.isNull() && requested.isValid() && !requested.isEmpty())
        image = image.scaled(requested, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return image;
}

QString Backend::folder() const {
    const QString remembered = QSettings().value("folder").toString();
    if (QDir(remembered).exists())
        return remembered;
    const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return QDir(documents).exists() ? documents : QDir::homePath();
}

void Backend::rememberFolder(const QString &path) { QSettings().setValue("folder", QFileInfo(path).absolutePath()); }

void Backend::newProject() {
    m_picker->save("new", tr("New Project"), folder() + "/" + tr("Punch list") + ".corbel", projectFilter());
}

void Backend::openProject() { m_picker->open("open", tr("Open Project"), {projectFilter()}, false, folder()); }

void Backend::importPlan() {
    if (!isOpen())
        return;
    m_picker->open("plan", tr("Import Floor Plan"),
                   {{tr("Plans"), {"*.pdf", "*.png", "*.jpg", "*.jpeg", "*.webp"}, {"application/pdf", "image/*"}}},
                   false, folder());
}

void Backend::addPhotos() {
    if (!m_selectedId)
        return;
    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    m_picker->open("photos", tr("Add Photos"), {{tr("Images"), {"*.jpg", "*.jpeg", "*.png", "*.webp", "*.heic"}, {"image/*"}}},
                   true, QDir(pictures).exists() ? pictures : folder());
}

void Backend::exportReport() {
    if (!isOpen())
        return;
    const QString base = QFileInfo(m_store.path()).absolutePath() + "/" + tr("%1 punch list").arg(m_name) + ".pdf";
    m_picker->save("export", tr("Export Report"), base, {tr("PDF documents"), {"*.pdf"}, {"application/pdf"}});
}

void Backend::pickerSelected(const QString &purpose, const QList<QUrl> &urls) {
    QStringList paths;
    for (const auto &url : urls)
        paths << (url.isLocalFile() ? url.toLocalFile() : url.toString());
    if (paths.isEmpty())
        return;
    if (purpose == "new" && createProjectAt(paths.first()))
        importPlan();
    else if (purpose == "open")
        openProjectAt(paths.first());
    else if (purpose == "plan")
        importPlanFrom(paths.first());
    else if (purpose == "photos")
        addPhotosFrom(paths);
    else if (purpose == "export")
        exportReportTo(paths.first());
}

bool Backend::createProjectAt(const QString &path) {
    QString target = path;
    if (!target.endsWith(".corbel"))
        target += ".corbel";
    Store store;
    if (!store.create(target, QFileInfo(target).completeBaseName())) {
        setError(store.error());
        return false;
    }
    store.close();
    return openProjectAt(target);
}

bool Backend::openProjectAt(const QString &path) {
    // Opened on the side first, so a file that isn't a project leaves the
    // current one open.
    Store probe;
    if (!probe.open(path)) {
        setError(probe.error());
        return false;
    }
    probe.close();
    m_store.open(path);
    m_name = m_store.name();
    ++m_generation;
    m_planCache.clear();
    m_undo.clear();
    m_redo.clear();
    m_selectedId = 0;
    m_tradeFilter.clear();
    m_assigneeFilter.clear();
    m_floorFilter = 0;
    m_statusFilter = -1;
    const auto plans = m_store.plans();
    m_currentPlan = plans.isEmpty() ? 0 : plans.first().id;
    rememberFolder(m_store.path());
    QSettings().setValue("lastProject", m_store.path());
    clearError();
    setNotice({});
    emit projectChanged();
    emit currentPlanChanged();
    emit filtersChanged();
    emit historyChanged();
    refresh();
    return true;
}

void Backend::reopenLastProject() {
    const QString last = QSettings().value("lastProject").toString();
    if (!last.isEmpty() && QFileInfo::exists(last))
        openProjectAt(last);
}

bool Backend::importPlanFrom(const QString &path) {
    if (!isOpen())
        return false;
    QString error;
    const auto pages = loadPlan(path, &error);
    if (pages.isEmpty()) {
        setError(error);
        return false;
    }
    int first = 0;
    for (const auto &page : pages) {
        const int id = m_store.addPlan(page.name, page.image);
        if (!id) {
            setError(m_store.error());
            break;
        }
        first = first ? first : id;
    }
    rememberFolder(path);
    if (first) {
        clearError();
        m_currentPlan = first;
        emit currentPlanChanged();
    }
    refresh();
    return first != 0;
}

bool Backend::addPhotosFrom(const QStringList &paths) {
    const int id = m_selectedId;
    if (!id)
        return false;
    const QVariantMap before = m_store.snapshot(id, true);
    QStringList failures;
    for (const auto &path : paths) {
        QString error;
        const QByteArray jpeg = loadPhoto(path, &error);
        if (jpeg.isEmpty() || !m_store.addPhoto(id, QFileInfo(path).fileName(), jpeg))
            failures << (error.isEmpty() ? m_store.error() : error);
    }
    if (!paths.isEmpty())
        rememberFolder(paths.first());
    record(id, before, true);
    if (failures.isEmpty())
        clearError();
    else
        setError(failures.join("\n"));
    refresh();
    return failures.isEmpty();
}

bool Backend::exportReportTo(const QString &path) {
    if (!isOpen())
        return false;
    QString target = path;
    if (!target.endsWith(".pdf", Qt::CaseInsensitive))
        target += ".pdf";
    QString error;
    if (!writeReport(m_store, target, &error)) {
        setError(error);
        return false;
    }
    clearError();
    m_lastExport = target;
    setNotice(tr("Report saved to %1").arg(QFileInfo(target).fileName()));
    return true;
}

void Backend::openExport() {
    if (!m_lastExport.isEmpty())
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_lastExport));
}

int Backend::addItem(double x, double y) {
    if (!isOpen() || !m_currentPlan)
        return 0;
    Item item;
    item.planId = m_currentPlan;
    item.number = m_store.nextNumber();
    item.x = qBound(0.0, x, 1.0);
    item.y = qBound(0.0, y, 1.0);
    item.trade = m_tradeFilter;
    item.assignee = m_assigneeFilter;
    item.created = QDateTime::currentDateTime();
    const int id = m_store.insertItem(item);
    if (!id) {
        setError(m_store.error());
        return 0;
    }
    record(id, {}, false);
    m_selectedId = id;
    refresh();
    emit itemAdded(id);
    return id;
}

void Backend::setField(int id, const QString &field, const QVariant &value) {
    Item item = m_store.item(id);
    if (!item.id)
        return;
    const QVariantMap before = m_store.snapshot(id, false);
    if (field == "title") item.title = value.toString().trimmed();
    else if (field == "trade") item.trade = value.toString().trimmed();
    else if (field == "assignee") item.assignee = value.toString().trimmed();
    else if (field == "notes") item.notes = value.toString();
    else if (field == "status") item.status = qBound(0, value.toInt(), 2);
    else if (field == "due") item.due = value.toDate();
    else return;
    if (Store::itemToMap(item) == Store::itemToMap(Store::itemFromMap(before)))
        return;
    if (!m_store.updateItem(item)) {
        setError(m_store.error());
        return;
    }
    record(id, before, false);
    refresh();
}

void Backend::moveItem(int id, double x, double y) {
    Item item = m_store.item(id);
    if (!item.id)
        return;
    const QVariantMap before = m_store.snapshot(id, false);
    item.x = qBound(0.0, x, 1.0);
    item.y = qBound(0.0, y, 1.0);
    if (!m_store.updateItem(item)) {
        setError(m_store.error());
        return;
    }
    record(id, before, false);
    refresh();
}

void Backend::removeItem(int id) {
    const QVariantMap before = m_store.snapshot(id, true);
    if (before.isEmpty())
        return;
    if (!m_store.removeItem(id)) {
        setError(m_store.error());
        return;
    }
    record(id, before, true);
    if (m_selectedId == id)
        m_selectedId = 0;
    refresh();
}

void Backend::removePhoto(int photoId) {
    const int id = m_selectedId;
    const QVariantMap before = m_store.snapshot(id, true);
    if (before.isEmpty() || !m_store.removePhoto(photoId))
        return;
    record(id, before, true);
    refresh();
}

void Backend::renamePlan(int id, const QString &name) {
    if (name.trimmed().isEmpty() || !m_store.renamePlan(id, name.trimmed()))
        return;
    refresh();
}

void Backend::select(int id) {
    if (id == m_selectedId)
        return;
    m_selectedId = id;
    for (const auto &item : m_all)
        if (item.id == id && item.planId != m_currentPlan) {
            m_currentPlan = item.planId;
            emit currentPlanChanged();
        }
    refresh();
}

// Steps through the list as it is filtered, for the arrow keys.
void Backend::selectRelative(int step) {
    const int count = m_items.rowCount();
    if (!count)
        return;
    const int index = m_items.indexOf(m_selectedId);
    const int next = index < 0 ? (step > 0 ? 0 : count - 1) : qBound(0, index + step, count - 1);
    select(m_items.data(m_items.index(next), ItemModel::IdRole).toInt());
}

void Backend::record(int id, const QVariantMap &before, bool withPhotos) {
    m_undo.append({id, before, m_store.snapshot(id, withPhotos)});
    m_redo.clear();
    emit historyChanged();
}

void Backend::apply(const Change &change, bool forward) {
    if (!m_store.restore(change.id, forward ? change.after : change.before)) {
        setError(m_store.error());
        return;
    }
    // Land on what changed, so the undo is visible.
    const Item item = m_store.item(change.id);
    m_selectedId = item.id;
    if (item.id && item.planId != m_currentPlan) {
        m_currentPlan = item.planId;
        emit currentPlanChanged();
    }
    refresh();
}

void Backend::undo() {
    if (m_undo.isEmpty())
        return;
    const Change change = m_undo.takeLast();
    m_redo.append(change);
    apply(change, false);
    emit historyChanged();
}

void Backend::redo() {
    if (m_redo.isEmpty())
        return;
    const Change change = m_redo.takeLast();
    m_undo.append(change);
    apply(change, true);
    emit historyChanged();
}

void Backend::setError(const QString &message) {
    if (message == m_error)
        return;
    m_error = message;
    emit errorChanged();
}

void Backend::clearError() { setError({}); }

void Backend::setNotice(const QString &message) {
    m_notice = message;
    emit noticeChanged();
}

ProjectImageProvider::ProjectImageProvider(Backend *backend, bool photos)
    : QQuickImageProvider(QQuickImageProvider::Image), m_backend(backend), m_photos(photos) {}

QImage ProjectImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize) {
    const int key = id.section('/', 1, 1).toInt();
    const QImage image = m_photos ? m_backend->photoImage(key, requestedSize) : m_backend->planImage(key);
    if (size)
        *size = image.size();
    return image;
}
