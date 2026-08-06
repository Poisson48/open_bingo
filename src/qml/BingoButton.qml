import QtQuick
import QtQuick.Controls

Button {
    id: btn
    property bool primary: false
    property bool danger: false

    implicitHeight: Theme.touchTarget
    padding: primary ? 12 : 10

    contentItem: Label {
        text: btn.text
        font.pixelSize: primary ? 14 : 13
        font.weight: primary ? Font.DemiBold : Font.Normal
        color: primary ? Theme.onAccent
             : danger ? Theme.danger
             : Theme.textDim
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.radius
        color: primary
               ? (btn.down ? Theme.accentDim : Theme.accent)
               : (btn.hovered ? Theme.surfaceHigh : "transparent")
        border.color: primary ? "transparent" : Theme.outlineLight
        border.width: primary ? 0 : 1
    }
}
