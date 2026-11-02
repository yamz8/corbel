import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// Material makes every button a pill whatever the window says; this one takes
// the corners the theme gives the windows, like everything else.
Button {
    Material.roundedScale: theme.radius
}
