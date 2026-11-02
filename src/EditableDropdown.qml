import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// A ComboBox to type into. Material pads its text on the left only, so in a
// right-to-left language the text runs into the edge; this pads both sides.
ComboBox {
    id: box
    editable: true
    Binding {
        target: box.contentItem
        property: "rightPadding"
        value: box.Material.textFieldHorizontalPadding
    }
}
