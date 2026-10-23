#include "itemmodel.h"

int ItemModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_items.size(); }

QVariant ItemModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_items.size())
        return {};
    const Item &item = m_items[index.row()];
    switch (role) {
    case IdRole: return item.id;
    case NumberRole: return item.number;
    case TitleRole: return item.title;
    case TradeRole: return item.trade;
    case AssigneeRole: return item.assignee;
    case DueRole: return item.due;
    case OverdueRole: return item.due.isValid() && item.due < QDate::currentDate() && item.status != int(Status::Done);
    case StatusRole: return item.status;
    case XRole: return item.x;
    case YRole: return item.y;
    case PlanIdRole: return item.planId;
    case PlanNameRole: return m_planNames.value(item.planId);
    case PhotoCountRole: return m_photoCounts.value(item.id);
    }
    return {};
}

QHash<int, QByteArray> ItemModel::roleNames() const {
    return {{IdRole, "itemId"},       {NumberRole, "number"},     {TitleRole, "title"},
            {TradeRole, "trade"},     {AssigneeRole, "assignee"}, {DueRole, "due"},
            {OverdueRole, "overdue"}, {StatusRole, "status"},     {XRole, "x"},
            {YRole, "y"},             {PlanIdRole, "planId"},     {PlanNameRole, "planName"},
            {PhotoCountRole, "photoCount"}};
}

void ItemModel::setItems(const QList<Item> &items, const QHash<int, QString> &planNames,
                         const QHash<int, int> &photoCounts) {
    bool sameRows = items.size() == m_items.size();
    for (int i = 0; sameRows && i < items.size(); ++i)
        sameRows = items[i].id == m_items[i].id;

    m_planNames = planNames;
    m_photoCounts = photoCounts;
    if (sameRows) {
        m_items = items;
        if (!items.isEmpty())
            emit dataChanged(index(0), index(items.size() - 1));
        return;
    }
    beginResetModel();
    m_items = items;
    endResetModel();
    emit countChanged();
}

int ItemModel::indexOf(int id) const {
    for (int i = 0; i < m_items.size(); ++i)
        if (m_items[i].id == id)
            return i;
    return -1;
}
