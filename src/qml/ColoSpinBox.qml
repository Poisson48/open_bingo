import QtQuick
import QtQuick.Controls

SpinBox {
    id: control

    implicitHeight: Theme.touchTarget
    font.pixelSize: 14
    editable: true

    contentItem: TextInput {
        text: control.textFromValue(control.value, control.locale)
        font: control.font
        color: control.enabled ? Theme.text : Theme.textDim
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        selectByMouse: true
    }

    background: Rectangle {
        radius: Theme.radius
        color: Theme.inputBg
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.accent : Theme.outlineLight
    }

    up.indicator: Rectangle {
        x: control.mirrored ? 0 : parent.width - width
        height: parent.height
        implicitWidth: Theme.touchTarget
        implicitHeight: Theme.touchTarget
        color: control.up.pressed ? Theme.surfaceHigh : "transparent"
        radius: Theme.radius
        Text {
            anchors.centerIn: parent
            text: "+"
            color: Theme.text
            font.pixelSize: 16
            font.weight: Font.DemiBold
        }
    }

    down.indicator: Rectangle {
        x: control.mirrored ? parent.width - width : 0
        height: parent.height
        implicitWidth: Theme.touchTarget
        implicitHeight: Theme.touchTarget
        color: control.down.pressed ? Theme.surfaceHigh : "transparent"
        radius: Theme.radius
        Text {
            anchors.centerIn: parent
            text: "−"
            color: Theme.text
            font.pixelSize: 16
            font.weight: Font.DemiBold
        }
    }
}
