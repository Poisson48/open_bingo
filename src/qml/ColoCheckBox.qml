import QtQuick
import QtQuick.Controls

CheckBox {
    id: control

    implicitHeight: Theme.touchTarget
    spacing: 10
    font.pixelSize: 14

    indicator: Rectangle {
        implicitWidth: 22
        implicitHeight: 22
        x: control.leftPadding
        y: parent.height / 2 - height / 2
        radius: 4
        color: control.checked ? Theme.accent : Theme.inputBg
        border.width: control.checked ? 0 : 1
        border.color: Theme.outlineLight

        Icon {
            anchors.centerIn: parent
            visible: control.checked
            name: "check"
            size: 14
            color: Theme.onAccent
        }
    }

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.enabled ? Theme.text : Theme.textDim
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
        wrapMode: Text.WordWrap
    }
}
