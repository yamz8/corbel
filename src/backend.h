#pragma once

#include <QObject>
#include <QQuickImageProvider>
#include <QSize>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include "itemmodel.h"
#include "store.h"

class FilePicker;

// The open project as QML sees it: its plans and items, the current floor,
// the selection, the filters, and undo. Every change is written to the
// project file as it happens, so there is never anything unsaved.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isOpen READ isOpen NOTIFY projectChanged)
    Q_PROPERTY(QString projectName READ projectName NOTIFY projectChanged)
    Q_PROPERTY(QString projectPath READ projectPath NOTIFY projectChanged)
    Q_PROPERTY(int generation READ generation NOTIFY imagesChanged)
    Q_PROPERTY(QVariantList plans READ plans NOTIFY plansChanged)
    Q_PROPERTY(int currentPlan READ currentPlan WRITE setCurrentPlan NOTIFY currentPlanChanged)
    Q_PROPERTY(QSize currentPlanSize READ currentPlanSize NOTIFY currentPlanChanged)
    Q_PROPERTY(ItemModel *items READ items CONSTANT)
    Q_PROPERTY(ItemModel *pins READ pins CONSTANT)
    Q_PROPERTY(int selectedId READ selectedId WRITE select NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selected READ selected NOTIFY selectionChanged)
    Q_PROPERTY(QStringList trades READ trades NOTIFY itemsChanged)
    Q_PROPERTY(QStringList usedTrades READ usedTrades NOTIFY itemsChanged)
    Q_PROPERTY(QStringList assignees READ assignees NOTIFY itemsChanged)
    Q_PROPERTY(QVariantList statusCounts READ statusCounts NOTIFY itemsChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY itemsChanged)
    // How many items a report exported now would hold.
    Q_PROPERTY(int reportCount READ reportCount NOTIFY itemsChanged)
    Q_PROPERTY(QString tradeFilter READ tradeFilter WRITE setTradeFilter NOTIFY filtersChanged)
    Q_PROPERTY(QString assigneeFilter READ assigneeFilter WRITE setAssigneeFilter NOTIFY filtersChanged)
    Q_PROPERTY(int floorFilter READ floorFilter WRITE setFloorFilter NOTIFY filtersChanged)
    Q_PROPERTY(int statusFilter READ statusFilter WRITE setStatusFilter NOTIFY filtersChanged)
    Q_PROPERTY(bool filtered READ filtered NOTIFY filtersChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString notice READ notice NOTIFY noticeChanged)
    Q_PROPERTY(QString lastExport READ lastExport NOTIFY noticeChanged)

public:
    explicit Backend(FilePicker *picker, QObject *parent = nullptr);
    ~Backend() override;

    bool isOpen() const { return m_store.isOpen(); }
    QString projectName() const { return m_name; }
    QString projectPath() const { return m_store.path(); }
    int generation() const { return m_generation; }
    QVariantList plans() const;
    int currentPlan() const { return m_currentPlan; }
    void setCurrentPlan(int id);
    QSize currentPlanSize() const;
    ItemModel *items() { return &m_items; }
    ItemModel *pins() { return &m_pins; }
    int selectedId() const { return m_selectedId; }
    QVariantMap selected() const;
    QStringList trades() const;
    QStringList usedTrades() const;
    QStringList assignees() const;
    QVariantList statusCounts() const;
    int totalCount() const { return m_all.size(); }
    int reportCount() const;
    QString tradeFilter() const { return m_filter.trade; }
    void setTradeFilter(const QString &trade);
    QString assigneeFilter() const { return m_filter.assignee; }
    void setAssigneeFilter(const QString &assignee);
    int floorFilter() const { return m_filter.floor; }
    void setFloorFilter(int planId);
    int statusFilter() const { return m_filter.status; }
    void setStatusFilter(int status);
    bool filtered() const { return !m_filter.isEmpty(); }
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    QString error() const { return m_error; }
    QString notice() const { return m_notice; }
    QString lastExport() const { return m_lastExport; }

    const Store &store() const { return m_store; }
    QImage planImage(int id);
    QImage photoImage(int id, const QSize &requested);

    // Pickers: each asks the desktop's file chooser, then acts on the answer.
    Q_INVOKABLE void newProject();
    Q_INVOKABLE void openProject();
    Q_INVOKABLE void importPlan();
    Q_INVOKABLE void addPhotos();
    Q_INVOKABLE void exportReport();

    Q_INVOKABLE bool createProjectAt(const QString &path);
    Q_INVOKABLE bool openProjectAt(const QString &path);
    Q_INVOKABLE bool importPlanFrom(const QString &path);
    Q_INVOKABLE bool addPhotosFrom(const QStringList &paths);
    Q_INVOKABLE bool exportReportTo(const QString &path);
    Q_INVOKABLE void reopenLastProject();

    Q_INVOKABLE int addItem(double x, double y);
    Q_INVOKABLE void setField(int id, const QString &field, const QVariant &value);
    Q_INVOKABLE void moveItem(int id, double x, double y);
    Q_INVOKABLE void removeItem(int id);
    Q_INVOKABLE void removePhoto(int photoId);
    Q_INVOKABLE void renamePlan(int id, const QString &name);
    Q_INVOKABLE void removePlan(int id);
    Q_INVOKABLE void select(int id);
    Q_INVOKABLE void selectRelative(int step);
    Q_INVOKABLE void clearFilters();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void clearError();
    Q_INVOKABLE void clearNotice() { setNotice({}); }
    Q_INVOKABLE void openExport();

signals:
    void projectChanged();
    void plansChanged();
    void currentPlanChanged();
    void itemsChanged();
    void selectionChanged();
    void filtersChanged();
    void historyChanged();
    void errorChanged();
    void noticeChanged();
    void imagesChanged();
    // Asks QML to put keyboard focus on the new item's title.
    void itemAdded(int id);

private:
    // One undoable step changes an item or a floor; a change is the steps
    // one action took (importing a PDF adds a floor per page).
    struct Step {
        bool plan;
        int id;
        QVariantMap before;
        QVariantMap after;
    };
    using Change = QList<Step>;

    void pickerSelected(const QString &purpose, const QList<QUrl> &urls);
    void setError(const QString &message);
    void setNotice(const QString &message);
    void record(int id, const QVariantMap &before, bool withPhotos);
    void push(const Change &change);
    void setFilter(const ItemFilter &filter);
    void forgetImages();
    void showPlan(int id);
    void apply(const Change &change, bool forward);
    void refresh();
    bool matches(const Item &item, bool ignoreFloor) const;
    QString folder() const;
    void rememberFolder(const QString &path);

    FilePicker *m_picker;
    Store m_store;
    QString m_name;
    int m_generation = 0;
    int m_currentPlan = 0;
    int m_selectedId = 0;
    QList<Item> m_all;
    QHash<int, int> m_photoCounts;
    ItemModel m_items;
    ItemModel m_pins;
    ItemFilter m_filter;
    QList<Change> m_undo;
    QList<Change> m_redo;
    QString m_error;
    QString m_notice;
    QString m_lastExport;
    QHash<int, QImage> m_planCache;
};

// Serves plans and photos to QML as image://plan/<generation>/<id> and
// image://photo/<generation>/<id>; the generation keeps a reopened project
// from showing another project's cached pictures.
class ProjectImageProvider : public QQuickImageProvider {
public:
    ProjectImageProvider(Backend *backend, bool photos);
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    Backend *m_backend;
    bool m_photos;
};
