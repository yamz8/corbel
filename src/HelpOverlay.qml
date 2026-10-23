import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Every shortcut, shown with ?.
Rectangle {
    id: overlay
    color: Qt.rgba(0, 0, 0, 0.72)

    readonly property var shortcuts: [
        ["Space", qsTr("Pin a defect under the pointer")],
        [qsTr("Click"), qsTr("Pin a defect where you click")],
        ["↑ ↓  J K", qsTr("Previous or next item")],
        ["Enter", qsTr("Edit the title")],
        ["1 2 3", qsTr("Open, in progress, done")],
        ["P", qsTr("Add photos")],
        ["Delete", qsTr("Delete the item")],
        ["Esc", qsTr("Stop editing, then deselect")],
        ["[  ]", qsTr("Previous or next floor")],
        ["+ − 0", qsTr("Zoom in, out, to fit")],
        ["Ctrl + " + qsTr("wheel"), qsTr("Zoom at the pointer")],
        ["I", qsTr("Import a floor plan")],
        ["Ctrl + E", qsTr("Export the report")],
        ["Ctrl + N", qsTr("New project")],
        ["Ctrl + O", qsTr("Open project")],
        ["Ctrl + Z", qsTr("Undo")],
        ["Ctrl + Shift + Z", qsTr("Redo")],
        ["?", qsTr("Show or hide this help")],
        ["Q", qsTr("Quit")],
    ]

    MouseArea { anchors.fill: parent; onClicked: win.helpVisible = false }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 560)
        height: content.implicitHeight + 48
        color: win.panel
        radius: theme.radius
        border.color: win.line

        ColumnLayout {
            id: content
            anchors.fill: parent
            anchors.margins: 24
            spacing: 14
            Label {
                text: qsTr("Keyboard shortcuts")
                font.pixelSize: 20
                font.weight: Font.DemiBold
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 24
                rowSpacing: 7
                Repeater {
                    model: overlay.shortcuts.length * 2
                    delegate: Label {
                        required property int index
                        readonly property var entry: overlay.shortcuts[Math.floor(index / 2)]
                        Layout.fillWidth: index % 2 === 1
                        text: entry[index % 2]
                        color: index % 2 === 0 ? theme.accent : Material.foreground
                        font.family: index % 2 === 0 ? "monospace" : Qt.application.font.family
                        wrapMode: Text.Wrap
                    }
                }
            }
        }
    }
}
