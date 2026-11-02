import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// The selected item's details. Text is saved when it's done being edited
// (Enter, leaving the field, or picking another item); everything else is
// saved as it is chosen.
Rectangle {
    id: editor
    color: win.panel

    readonly property var item: backend.selected
    readonly property int itemId: item.id ?? 0

    function commit() {
        if (!itemId) return
        if (titleField.text.trim() !== (item.title ?? "")) backend.setField(itemId, "title", titleField.text)
        if (tradeBox.editText.trim() !== (item.trade ?? "")) backend.setField(itemId, "trade", tradeBox.editText)
        if (assigneeBox.editText.trim() !== (item.assignee ?? "")) backend.setField(itemId, "assignee", assigneeBox.editText)
        if (notesArea.text !== (item.notes ?? "")) backend.setField(itemId, "notes", notesArea.text)
    }
    function focusTitle() {
        titleField.forceActiveFocus()
        titleField.selectAll()
    }
    // Another item replaces every field. For the same item, a field being
    // typed in keeps what is typed and the rest follow it (an undo, say).
    property int loadedId: 0
    function load() {
        const other = itemId !== loadedId
        loadedId = itemId
        if (other && editor.activeFocusInside()) planView.forceActiveFocus()
        if (other || !titleField.activeFocus) titleField.text = item.title ?? ""
        if (other || !tradeBox.activeFocus) tradeBox.editText = item.trade ?? ""
        if (other || !assigneeBox.activeFocus) assigneeBox.editText = item.assignee ?? ""
        if (other || !notesArea.activeFocus) notesArea.text = item.notes ?? ""
    }
    function activeFocusInside() {
        return titleField.activeFocus || tradeBox.activeFocus || assigneeBox.activeFocus || notesArea.activeFocus
    }
    onItemChanged: load()

    // A short window scrolls the details rather than cutting off the photos.
    Flickable {
        id: scroller
        objectName: "editorScroller"
        anchors.fill: parent
        clip: true
        contentWidth: width
        contentHeight: Math.max(height, column.implicitHeight + 32)
        // Only a short window scrolls, so a drag to select text otherwise stays
        // a selection.
        interactive: contentHeight > height
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: column
            x: 16
            y: 16
            width: scroller.width - 32
            height: scroller.contentHeight - 32
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: "#" + (editor.item.number ?? "")
                    font.pixelSize: 22
                    font.weight: Font.DemiBold
                }
                Label {
                    Layout.fillWidth: true
                    text: editor.item.planName ?? ""
                    color: win.muted
                    elide: Text.ElideRight
                }
                ToolButton {
                    text: qsTr("Delete")
                    Material.foreground: theme.open
                    onClicked: backend.removeItem(editor.itemId)
                }
                ToolButton {
                    text: "×"
                    font.pixelSize: 18
                    onClicked: win.selectItem(0)
                }
            }

            TextField {
                id: titleField
                objectName: "titleField"
                Layout.fillWidth: true
                placeholderText: win.label(titleField, qsTr("What needs fixing?"))
                font.pixelSize: 16
                onEditingFinished: editor.commit()
                // Enter is done typing, so the single-key shortcuts work again.
                onAccepted: planView.forceActiveFocus()
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Repeater {
                    model: 3
                    delegate: ActionButton {
                        required property int index
                        readonly property bool current: (editor.item.status ?? 0) === index
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        text: win.statusName(index)
                        leftPadding: 6
                        rightPadding: 6
                        checkable: true
                        checked: current
                        Material.background: current ? win.statusColor(index) : "#202026"
                        Material.foreground: current ? (win.luminance(win.statusColor(index)) > 0.5 ? "black" : "white") : Material.foreground
                        onClicked: { editor.commit(); backend.setField(editor.itemId, "status", index) }
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 12
                rowSpacing: 4

                Label { text: qsTr("Trade"); color: win.muted }
                EditableDropdown {
                    id: tradeBox
                    Layout.fillWidth: true
                    model: backend.trades
                    onActivated: backend.setField(editor.itemId, "trade", currentText)
                    onAccepted: editor.commit()
                    onActiveFocusChanged: if (!activeFocus) editor.commit()
                }
                Label { text: qsTr("Assignee"); color: win.muted }
                EditableDropdown {
                    id: assigneeBox
                    Layout.fillWidth: true
                    model: backend.assignees
                    onActivated: backend.setField(editor.itemId, "assignee", currentText)
                    onAccepted: editor.commit()
                    onActiveFocusChanged: if (!activeFocus) editor.commit()
                }
                Label { text: qsTr("Due"); color: win.muted }
                RowLayout {
                    Layout.fillWidth: true
                    ActionButton {
                        objectName: "dueButton"
                        Layout.fillWidth: true
                        flat: true
                        text: win.formatDate(editor.item.due) || qsTr("No due date")
                        onClicked: datePopup.openAt(editor.item.due)
                    }
                    ToolButton {
                        visible: !isNaN(editor.item.due)
                        text: "×"
                        onClicked: backend.setField(editor.itemId, "due", undefined)
                    }
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: 110
                TextArea {
                    id: notesArea
                    objectName: "notesField"
                    placeholderText: win.label(notesArea, qsTr("Notes"))
                    wrapMode: TextEdit.Wrap
                    onActiveFocusChanged: if (!activeFocus) editor.commit()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Photos")
                    font.weight: Font.Medium
                }
                ActionButton {
                    text: qsTr("Add photos")
                    flat: true
                    onClicked: { editor.commit(); backend.addPhotos() }
                }
            }

            GridView {
                id: photos
                objectName: "photoGrid"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: cellHeight
                clip: true
                cellWidth: width / 3
                cellHeight: cellWidth * 0.78
                model: editor.item.photos ?? []
                boundsBehavior: Flickable.StopAtBounds
                delegate: Item {
                    id: cell
                    required property var modelData
                    required property int index
                    width: photos.cellWidth
                    height: photos.cellHeight
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 3
                        radius: Math.min(4, theme.radius)
                        color: "#0b0b0d"
                        clip: true
                        Image {
                            anchors.fill: parent
                            fillMode: Image.PreserveAspectCrop
                            source: "image://photo/" + backend.generation + "/" + cell.modelData.id
                            sourceSize: Qt.size(320, 320)
                        }
                        MouseArea {
                            id: photoArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: photoViewer.show(cell.index)
                        }
                        ToolButton {
                            anchors.top: parent.top
                            anchors.right: parent.right
                            visible: photoArea.containsMouse || hovered
                            text: "×"
                            onClicked: backend.removePhoto(cell.modelData.id)
                        }
                    }
                }
                Label {
                    anchors.centerIn: parent
                    visible: photos.count === 0
                    color: win.muted
                    text: qsTr("No photos")
                }
            }
        }
    }

    DatePopup {
        id: datePopup
        objectName: "datePopup"
        onPicked: (date) => backend.setField(editor.itemId, "due", date)
    }

    Popup {
        id: photoViewer
        objectName: "photoViewer"
        property int index: 0
        readonly property var list: editor.item.photos ?? []
        function show(i) { index = i; open() }
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: parent ? parent.width - 80 : 800
        height: parent ? parent.height - 80 : 600
        modal: true
        padding: 0
        background: Rectangle { color: "#0b0b0d"; radius: theme.radius }
        Image {
            anchors.fill: parent
            anchors.margins: 12
            fillMode: Image.PreserveAspectFit
            source: photoViewer.visible && photoViewer.list.length > photoViewer.index
                    ? "image://photo/" + backend.generation + "/" + photoViewer.list[photoViewer.index].id : ""
        }
        Label {
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottomMargin: 16
            text: (photoViewer.index + 1).toLocaleString(Qt.locale(), 'f', 0) + " / "
                  + photoViewer.list.length.toLocaleString(Qt.locale(), 'f', 0)
        }
        MouseArea { anchors.fill: parent; onClicked: photoViewer.close() }
        Shortcut { sequence: "Right"; enabled: photoViewer.visible; onActivated: photoViewer.index = Math.min(photoViewer.list.length - 1, photoViewer.index + 1) }
        Shortcut { sequence: "Left"; enabled: photoViewer.visible; onActivated: photoViewer.index = Math.max(0, photoViewer.index - 1) }
    }
}
