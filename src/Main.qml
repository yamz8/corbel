import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1440
    height: 880
    minimumWidth: 960
    minimumHeight: 560
    visible: true
    title: backend.isOpen ? backend.projectName + " — Corbel" : "Corbel"
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    readonly property color panel: "#16161a"
    readonly property color line: "#2a2a31"
    readonly property color muted: "#9a9aa3"
    property bool helpVisible: false
    // Single-key shortcuts stand aside while a text field has the keyboard.
    readonly property bool typing: activeFocusItem !== null && activeFocusItem.hasOwnProperty("cursorPosition")
    readonly property bool hasPlan: backend.isOpen && backend.plans.length > 0
    readonly property bool rtl: Qt.application.layoutDirection === Qt.RightToLeft
    // Material's outlined fields leave the gap for a floating label on the
    // left even in right-to-left languages, so there the label only shows
    // while the field is empty and idle.
    function label(field, text) {
        return rtl && (field.activeFocus || field.text.length > 0) ? "" : text
    }

    function statusColor(status) {
        return [theme.open, theme.progress, theme.done][Math.max(0, Math.min(2, status))]
    }
    function statusName(status) {
        return [qsTr("Open"), qsTr("In progress"), qsTr("Done")][Math.max(0, Math.min(2, status))]
    }
    // Every change of selection goes through here, so an edit in progress is
    // saved to the item it was typed for.
    function selectItem(id) {
        editor.commit()
        backend.select(id)
    }
    function formatDate(date) {
        translator.language
        return date && !isNaN(date) ? date.toLocaleDateString(Qt.locale(), Locale.ShortFormat) : ""
    }

    Shortcut { sequences: ["?", "Shift+?"]; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: Qt.quit() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; enabled: !win.typing && win.hasPlan; onActivated: planView.addAtPointer() }
    Shortcut { sequences: [StandardKey.New]; context: Qt.ApplicationShortcut; onActivated: backend.newProject() }
    Shortcut { sequences: [StandardKey.Open]; context: Qt.ApplicationShortcut; onActivated: backend.openProject() }
    Shortcut { sequence: "I"; context: Qt.ApplicationShortcut; enabled: !win.typing && backend.isOpen; onActivated: backend.importPlan() }
    Shortcut { sequence: "Ctrl+E"; context: Qt.ApplicationShortcut; enabled: backend.isOpen; onActivated: backend.exportReport() }
    Shortcut { sequence: "P"; context: Qt.ApplicationShortcut; enabled: !win.typing && backend.selectedId > 0; onActivated: backend.addPhotos() }
    Shortcut { sequences: [StandardKey.Undo]; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: { editor.commit(); backend.undo() } }
    Shortcut { sequences: [StandardKey.Redo, "Ctrl+Shift+Z"]; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: backend.redo() }
    Shortcut { sequences: [StandardKey.Delete, "Backspace"]; context: Qt.ApplicationShortcut; enabled: !win.typing && backend.selectedId > 0; onActivated: backend.removeItem(backend.selectedId) }
    Shortcut { sequences: ["Down", "J"]; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: { editor.commit(); backend.selectRelative(1) } }
    Shortcut { sequences: ["Up", "K"]; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: { editor.commit(); backend.selectRelative(-1) } }
    Shortcut { sequence: "1"; context: Qt.ApplicationShortcut; enabled: !win.typing && backend.selectedId > 0; onActivated: backend.setField(backend.selectedId, "status", 0) }
    Shortcut { sequence: "2"; context: Qt.ApplicationShortcut; enabled: !win.typing && backend.selectedId > 0; onActivated: backend.setField(backend.selectedId, "status", 1) }
    Shortcut { sequence: "3"; context: Qt.ApplicationShortcut; enabled: !win.typing && backend.selectedId > 0; onActivated: backend.setField(backend.selectedId, "status", 2) }
    Shortcut { sequence: "Return"; context: Qt.ApplicationShortcut; enabled: !win.typing && backend.selectedId > 0; onActivated: editor.focusTitle() }
    Shortcut { sequence: "]"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: floorBar.step(1) }
    Shortcut { sequence: "["; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: floorBar.step(-1) }
    Shortcut { sequences: ["+", "="]; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: planView.zoomBy(1.25) }
    Shortcut { sequence: "-"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: planView.zoomBy(0.8) }
    Shortcut { sequence: "0"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: planView.fit() }
    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (win.helpVisible) win.helpVisible = false
            else if (win.typing) { editor.commit(); planView.forceActiveFocus() }
            else win.selectItem(0)
        }
    }

    Connections {
        target: backend
        function onItemAdded(id) { editor.focusTitle() }
    }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        objectName: "root"
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // Top bar: the project, its floors, and the report.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 56
                color: win.panel
                visible: backend.isOpen

                RowLayout {
                    id: floorBar
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 12
                    spacing: 10

                    function step(delta) {
                        const plans = backend.plans
                        if (plans.length < 2) return
                        let index = plans.findIndex(p => p.id === backend.currentPlan)
                        index = (index + delta + plans.length) % plans.length
                        backend.currentPlan = plans[index].id
                    }

                    Label {
                        text: backend.projectName
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                        Layout.maximumWidth: 260
                    }
                    Rectangle { width: 1; height: 24; color: win.line }
                    Dropdown {
                        id: floorBox
                        visible: win.hasPlan
                        Layout.preferredWidth: 260
                        model: backend.plans
                        textRole: "name"
                        valueRole: "id"
                        currentIndex: backend.plans.findIndex(p => p.id === backend.currentPlan)
                        onActivated: backend.currentPlan = currentValue
                        displayText: currentIndex >= 0 ? backend.plans[currentIndex].name : ""
                    }
                    ToolButton {
                        visible: win.hasPlan
                        text: qsTr("Rename")
                        onClicked: renamePopup.openFor(backend.currentPlan)
                    }
                    ToolButton {
                        visible: win.hasPlan
                        text: qsTr("Delete floor")
                        onClicked: deleteFloorPopup.openFor(backend.currentPlan)
                    }
                    ToolButton {
                        text: qsTr("Import plan")
                        onClicked: backend.importPlan()
                    }
                    Item { Layout.fillWidth: true }
                    Repeater {
                        model: 3
                        delegate: RowLayout {
                            required property int index
                            spacing: 6
                            Layout.rightMargin: 8
                            Rectangle {
                                width: 10; height: 10; radius: 5
                                color: win.statusColor(index)
                            }
                            Label {
                                text: (backend.statusCounts[index] ?? 0).toLocaleString(Qt.locale(), 'f', 0) + " " + win.statusName(index)
                                color: win.muted
                            }
                        }
                    }
                    ToolButton {
                        text: qsTr("Undo")
                        enabled: backend.canUndo
                        onClicked: { editor.commit(); backend.undo() }
                    }
                    Button {
                        text: backend.filtered ? qsTr("Export %n items", "", backend.reportCount) : qsTr("Export report")
                        highlighted: true
                        enabled: win.hasPlan
                        onClicked: { editor.commit(); backend.exportReport() }
                    }
                    ToolButton {
                        text: "?"
                        font.pixelSize: 16
                        onClicked: win.helpVisible = true
                    }
                }
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: win.line }
            }

            // Errors are said plainly, in the window.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: errorText.implicitHeight + 20
                visible: backend.error !== ""
                color: Qt.rgba(0.9, 0.28, 0.3, 0.18)
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 8
                    Label {
                        id: errorText
                        Layout.fillWidth: true
                        text: backend.error
                        wrapMode: Text.Wrap
                    }
                    ToolButton { text: "×"; font.pixelSize: 18; onClicked: backend.clearError() }
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                visible: backend.notice !== ""
                color: Qt.alpha(theme.accent, 0.16)
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 8
                    Label { Layout.fillWidth: true; text: backend.notice; elide: Text.ElideMiddle }
                    ToolButton { text: qsTr("View"); visible: backend.lastExport !== ""; onClicked: backend.openExport() }
                    ToolButton { text: "×"; font.pixelSize: 18; onClicked: noticeTimer.triggered() }
                }
                Timer { id: noticeTimer; interval: 8000; running: backend.notice !== ""; onTriggered: backend.clearNotice() }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0
                visible: win.hasPlan

                PlanView {
                    id: planView
                    objectName: "planView"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
                Rectangle { Layout.fillHeight: true; width: 1; color: win.line }
                ItemList {
                    id: itemList
                    objectName: "itemList"
                    Layout.preferredWidth: 360
                    Layout.fillHeight: true
                }
                Rectangle { Layout.fillHeight: true; width: 1; color: win.line; visible: editor.visible }
                ItemEditor {
                    id: editor
                    Layout.preferredWidth: 380
                    Layout.fillHeight: true
                    visible: backend.selectedId > 0
                }
            }

            // No project, or a project without a plan yet.
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: !win.hasPlan

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 18
                    width: Math.min(parent.width - 64, 460)

                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: backend.isOpen ? qsTr("Add a floor plan") : "Corbel"
                        font.pixelSize: 34
                        font.weight: Font.DemiBold
                    }
                    Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        color: win.muted
                        text: backend.isOpen
                              ? qsTr("Import a PDF or image of the floor plan. Every page of a PDF becomes a floor you can pin defects on.")
                              : qsTr("Punch lists on floor plans. Pin each defect where it is, track it to done, and hand over a closeout report.")
                    }
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 12
                        Button {
                            visible: backend.isOpen
                            highlighted: true
                            text: qsTr("Import plan")
                            onClicked: backend.importPlan()
                        }
                        Button {
                            visible: !backend.isOpen
                            highlighted: true
                            text: qsTr("New project")
                            onClicked: backend.newProject()
                        }
                        Button {
                            visible: !backend.isOpen
                            text: qsTr("Open project")
                            onClicked: backend.openProject()
                        }
                    }
                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        color: win.muted
                        text: qsTr("Press ? for keyboard shortcuts")
                    }
                }
            }
        }

        Popup {
            id: renamePopup
            property int planId: 0
            function openFor(id) {
                planId = id
                const plan = backend.plans.find(p => p.id === id)
                renameField.text = plan ? plan.name : ""
                open()
                renameField.selectAll()
                renameField.forceActiveFocus()
            }
            anchors.centerIn: parent
            modal: true
            padding: 20
            width: 380
            background: Rectangle { color: win.panel; radius: theme.radius; border.color: win.line }
            contentItem: ColumnLayout {
                spacing: 12
                TextField {
                    id: renameField
                    Layout.fillWidth: true
                    placeholderText: win.label(renameField, qsTr("Floor name"))
                    onAccepted: { backend.renamePlan(renamePopup.planId, text); renamePopup.close() }
                }
                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    Button { text: qsTr("Cancel"); flat: true; onClicked: renamePopup.close() }
                    Button {
                        text: qsTr("Rename")
                        highlighted: true
                        onClicked: { backend.renamePlan(renamePopup.planId, renameField.text); renamePopup.close() }
                    }
                }
            }
        }

        // Asked first, since a floor takes its items and photos with it; undo
        // still brings it all back.
        Popup {
            id: deleteFloorPopup
            objectName: "deleteFloorPopup"
            property int planId: 0
            property string name: ""
            property int count: 0
            function openFor(id) {
                const plan = backend.plans.find(p => p.id === id)
                if (!plan) return
                planId = id
                name = plan.name
                count = plan.count
                if (count === 0) { backend.removePlan(id); return }
                open()
            }
            anchors.centerIn: parent
            modal: true
            padding: 20
            width: 420
            background: Rectangle { color: win.panel; radius: theme.radius; border.color: win.line }
            contentItem: ColumnLayout {
                spacing: 14
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    font.pixelSize: 16
                    font.weight: Font.Medium
                    text: qsTr("Delete %1?").arg(deleteFloorPopup.name)
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: win.muted
                    text: qsTr("Its %n items and their photos are deleted with it. Ctrl+Z brings them back.", "", deleteFloorPopup.count)
                }
                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    Button { text: qsTr("Cancel"); flat: true; onClicked: deleteFloorPopup.close() }
                    Button {
                        objectName: "confirmDeleteFloor"
                        text: qsTr("Delete floor")
                        highlighted: true
                        Material.accent: theme.open
                        onClicked: { deleteFloorPopup.close(); backend.removePlan(deleteFloorPopup.planId) }
                    }
                }
            }
        }

        HelpOverlay {
            anchors.fill: parent
            visible: win.helpVisible
        }
    }
}
