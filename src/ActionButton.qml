import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// Material makes every button a pill whatever the window says; this one takes
// the corners the theme gives the windows, like everything else. Material also
// writes white on a highlighted button, which disappears on a light accent, so
// the label goes dark or light by how bright the button's own color is.
Button {
    id: button
    Material.roundedScale: theme.radius
    contentItem: Label {
        text: button.text
        font: button.font
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        color: !button.enabled ? button.Material.hintTextColor
             : button.highlighted && !button.flat ? (win.luminance(button.Material.accent) > 0.5 ? "black" : "white")
             : button.Material.foreground
    }
}
