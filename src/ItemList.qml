import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Corbel

// The punch list beside the plan, filtered by status, trade, assignee, and
// floor. Selecting here selects on the plan, and the other way round.
Rectangle {
    id: list
    color: win.panel

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 8
            rowSpacing: 0

            Dropdown {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                model: [qsTr("All statuses"), qsTr("Open"), qsTr("In progress"), qsTr("Done")]
                currentIndex: backend.statusFilter + 1
                onActivated: (index) => backend.statusFilter = index - 1
            }
            Dropdown {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                model: [qsTr("All floors")].concat(backend.plans.map(p => p.name))
                currentIndex: backend.floorFilter ? backend.plans.findIndex(p => p.id === backend.floorFilter) + 1 : 0
                onActivated: (index) => backend.floorFilter = index ? backend.plans[index - 1].id : 0
            }
            Dropdown {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                model: [qsTr("All trades")].concat(backend.usedTrades)
                currentIndex: backend.tradeFilter ? backend.usedTrades.indexOf(backend.tradeFilter) + 1 : 0
                onActivated: (index) => backend.tradeFilter = index ? backend.usedTrades[index - 1] : ""
            }
            Dropdown {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                model: [qsTr("All assignees")].concat(backend.assignees)
                currentIndex: backend.assigneeFilter ? backend.assignees.indexOf(backend.assigneeFilter) + 1 : 0
                onActivated: (index) => backend.assigneeFilter = index ? backend.assignees[index - 1] : ""
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                color: win.muted
                text: backend.filtered
                      ? qsTr("%1 of %2").arg(view.count.toLocaleString(Qt.locale(), 'f', 0)).arg(qsTr("%n items", "", backend.totalCount))
                      : qsTr("%n items", "", backend.totalCount)
            }
            Button {
                visible: backend.filtered
                flat: true
                text: qsTr("Clear filters")
                onClicked: backend.clearFilters()
            }
        }

        ListView {
            id: view
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: backend.items
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            currentIndex: backend.items.indexOf(backend.selectedId)
            onCurrentIndexChanged: if (currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Contain)
            highlightFollowsCurrentItem: false

            delegate: Rectangle {
                id: row
                objectName: "row" + model.number
                required property var model
                required property int index
                readonly property bool isSelected: model.itemId === backend.selectedId
                width: ListView.view.width
                height: 64
                radius: Math.min(4, theme.radius)
                color: isSelected ? Qt.alpha(theme.accent, 0.16) : hoverArea.containsMouse ? "#1e1e24" : "transparent"
                border.width: isSelected ? 1 : 0
                border.color: Qt.alpha(theme.accent, 0.5)

                MouseArea {
                    id: hoverArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: win.selectItem(row.model.itemId)
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 10
                    spacing: 10

                    Pin {
                        Layout.preferredWidth: 34
                        Layout.preferredHeight: 34
                        number: row.model.number
                        color: win.statusColor(row.model.status)
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label {
                            Layout.fillWidth: true
                            text: row.model.title || qsTr("Untitled")
                            color: row.model.title ? Material.foreground : win.muted
                            font.italic: !row.model.title
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            color: win.muted
                            font.pixelSize: 12
                            elide: Text.ElideRight
                            text: [row.model.trade, row.model.assignee, row.model.planName].filter(s => s).join(" · ")
                        }
                    }
                    ColumnLayout {
                        spacing: 2
                        Label {
                            Layout.alignment: Qt.AlignRight
                            visible: text !== ""
                            font.pixelSize: 12
                            color: row.model.overdue ? theme.open : win.muted
                            text: win.formatDate(row.model.due)
                        }
                        Label {
                            Layout.alignment: Qt.AlignRight
                            visible: row.model.photoCount > 0
                            font.pixelSize: 12
                            color: win.muted
                            text: qsTr("%n photos", "", row.model.photoCount)
                        }
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                width: parent.width - 40
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                visible: view.count === 0
                color: win.muted
                text: backend.totalCount === 0 ? qsTr("No items yet. Click the plan where something needs fixing.")
                                               : qsTr("Nothing matches these filters.")
            }
        }
    }
}
