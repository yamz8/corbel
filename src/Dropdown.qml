import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// A ComboBox to pick from. Material shows the choice in a text field, which
// cuts a long translation off mid-word; a label ends it with an ellipsis, and
// pads both sides so right-to-left text keeps clear of the edge too.
// A filter names what it filters while its first choice, "All …", is picked,
// which stays short in every language.
ComboBox {
    id: box
    property string category
    readonly property bool showsCategory: category !== "" && currentIndex === 0
    contentItem: Label {
        leftPadding: box.Material.textFieldHorizontalPadding
        rightPadding: box.Material.textFieldHorizontalPadding
        verticalAlignment: Text.AlignVCenter
        text: box.showsCategory ? box.category : box.displayText
        font: box.font
        elide: Text.ElideRight
        color: box.enabled && !box.showsCategory ? box.Material.foreground : box.Material.hintTextColor
    }
}
