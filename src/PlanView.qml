import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import Corbel

// The current floor plan with its pins. Click to pin a defect, drag a pin to
// move it, Ctrl+wheel or pinch to zoom, drag the plan to pan.
Item {
    id: view
    clip: true

    readonly property size planSize: backend.currentPlanSize
    readonly property real fitScale: planSize.width > 0
        ? Math.min(width / planSize.width, height / planSize.height) * 0.94 : 1
    readonly property real scale: planSize.width > 0 ? flick.contentWidth / planSize.width : 1
    property bool fitted: true
    readonly property int pinSize: 30

    function fit() {
        fitted = true
        flick.contentWidth = planSize.width * fitScale
        flick.contentHeight = planSize.height * fitScale
        flick.contentX = -flick.leftMargin
        flick.contentY = -flick.topMargin
    }
    // Zooms about a point in the view, keeping what is under it in place.
    function zoomAt(factor, px, py) {
        if (planSize.width <= 0) return
        const next = Math.max(fitScale * 0.6, Math.min(3, scale * factor))
        fitted = false
        flick.resizeContent(planSize.width * next, planSize.height * next,
                            Qt.point(flick.contentX + px, flick.contentY + py))
        flick.returnToBounds()
    }
    function zoomBy(factor) {
        hover.hovered ? zoomAt(factor, hover.point.position.x, hover.point.position.y)
                      : zoomAt(factor, width / 2, height / 2)
    }
    // New items land under the pointer when it's over the plan, otherwise in
    // the middle of what is visible.
    function addAtPointer() {
        let p
        if (hover.hovered)
            p = canvas.mapFromItem(view, hover.point.position.x, hover.point.position.y)
        else
            p = canvas.mapFromItem(view, width / 2, height / 2)
        win.selectItem(0)
        backend.addItem(p.x / canvas.width, p.y / canvas.height)
    }
    // Brings a pin into view when it is selected from the list.
    function reveal(id) {
        const index = backend.pins.indexOf(id)
        if (index < 0) return
        const pin = pins.itemAt(index)
        if (!pin) return
        const cx = pin.x + pin.width / 2, cy = pin.y + pin.height / 2
        const visible = cx > flick.contentX + 40 && cx < flick.contentX + flick.width - 40
                     && cy > flick.contentY + 40 && cy < flick.contentY + flick.height - 40
        if (visible) return
        flick.contentX = Math.max(-flick.leftMargin, Math.min(cx - flick.width / 2, flick.contentWidth - flick.width + flick.rightMargin))
        flick.contentY = Math.max(-flick.topMargin, Math.min(cy - flick.height / 2, flick.contentHeight - flick.height + flick.bottomMargin))
    }

    onFitScaleChanged: if (fitted) fit()
    onPlanSizeChanged: fit()

    Connections {
        target: backend
        function onSelectionChanged() { Qt.callLater(view.reveal, backend.selectedId) }
    }

    Rectangle { anchors.fill: parent; color: "#0b0b0d" }

    Flickable {
        id: flick
        anchors.fill: parent
        boundsBehavior: Flickable.StopAtBounds
        leftMargin: Math.max(0, (width - contentWidth) / 2)
        rightMargin: leftMargin
        topMargin: Math.max(0, (height - contentHeight) / 2)
        bottomMargin: topMargin
        ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        Item {
            id: canvas
            objectName: "planCanvas"
            width: flick.contentWidth
            height: flick.contentHeight

            Image {
                anchors.fill: parent
                source: backend.currentPlan > 0 ? "image://plan/" + backend.generation + "/" + backend.currentPlan : ""
                smooth: true
                mipmap: true
                cache: true
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.CrossCursor
                onClicked: (mouse) => {
                    win.selectItem(0)
                    backend.addItem(mouse.x / canvas.width, mouse.y / canvas.height)
                }
            }

            Repeater {
                id: pins
                model: backend.pins
                delegate: Pin {
                    id: pin
                    objectName: "pin" + model.number
                    required property var model
                    readonly property int itemId: model.itemId
                    // The model's x and y are fractions of the plan.
                    readonly property real px: model.x
                    readonly property real py: model.y
                    readonly property bool isSelected: itemId === backend.selectedId
                    width: view.pinSize * (isSelected ? 1.3 : 1) * 1.2
                    height: width
                    number: model.number
                    color: win.statusColor(model.status)
                    selected: isSelected
                    z: isSelected ? 2 : 1
                    Binding on x { value: pin.px * canvas.width - pin.width / 2; when: !drag.drag.active }
                    Binding on y { value: pin.py * canvas.height - pin.height / 2; when: !drag.drag.active }

                    MouseArea {
                        id: drag
                        anchors.fill: parent
                        cursorShape: drag.drag.active ? Qt.ClosedHandCursor : Qt.PointingHandCursor
                        preventStealing: true
                        hoverEnabled: true
                        drag.target: pin
                        drag.threshold: 4
                        drag.minimumX: -pin.width / 2
                        drag.maximumX: canvas.width - pin.width / 2
                        drag.minimumY: -pin.height / 2
                        drag.maximumY: canvas.height - pin.height / 2
                        onPressed: if (backend.selectedId !== pin.itemId) win.selectItem(pin.itemId)
                        onReleased: {
                            if (!drag.drag.active) return
                            backend.moveItem(pin.itemId, (pin.x + pin.width / 2) / canvas.width,
                                             (pin.y + pin.height / 2) / canvas.height)
                        }
                    }
                    ToolTip.visible: drag.containsMouse && !drag.drag.active
                    ToolTip.delay: 400
                    ToolTip.text: "#" + pin.number
                }
            }
        }

        WheelHandler {
            acceptedModifiers: Qt.ControlModifier
            onWheel: (event) => view.zoomAt(Math.pow(1.0015, event.angleDelta.y), point.position.x, point.position.y)
        }
        PinchHandler {
            target: null
            property real last: 1
            onActiveChanged: last = 1
            onActiveScaleChanged: {
                view.zoomAt(activeScale / last, centroid.position.x, centroid.position.y)
                last = activeScale
            }
        }
    }

    HoverHandler { id: hover }

    // Zoom controls and a hint, at the bottom of the plan.
    Row {
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 14
        spacing: 6
        Repeater {
            model: [{ label: "−", factor: 0.8 }, { label: qsTr("Fit"), factor: 0 }, { label: "+", factor: 1.25 }]
            delegate: Button {
                required property var modelData
                text: modelData.label
                width: modelData.factor === 0 ? implicitWidth : 44
                flat: false
                Material.background: "#202026"
                onClicked: modelData.factor === 0 ? view.fit() : view.zoomBy(modelData.factor)
            }
        }
    }
    Label {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: 18
        color: win.muted
        text: qsTr("Click the plan to pin a defect · Space pins under the pointer")
        visible: backend.pins.count === 0
    }
}
