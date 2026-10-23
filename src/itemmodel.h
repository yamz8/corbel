#pragma once

#include <QAbstractListModel>
#include <QHash>

#include "store.h"

// Items for a QML view. Refreshing with the same items in the same order
// only updates their fields, so an edit doesn't reset the view's scrolling.
class ItemModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        NumberRole,
        TitleRole,
        TradeRole,
        AssigneeRole,
        DueRole,
        OverdueRole,
        StatusRole,
        XRole,
        YRole,
        PlanIdRole,
        PlanNameRole,
        PhotoCountRole,
    };

    explicit ItemModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setItems(const QList<Item> &items, const QHash<int, QString> &planNames, const QHash<int, int> &photoCounts);
    Q_INVOKABLE int indexOf(int id) const;

signals:
    void countChanged();

private:
    QList<Item> m_items;
    QHash<int, QString> m_planNames;
    QHash<int, int> m_photoCounts;
};
