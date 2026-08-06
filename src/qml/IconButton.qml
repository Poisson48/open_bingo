import QtQuick
import QtQuick.Controls

// Bouton icône tactile — tracés vectoriels, pas de glyphes Unicode.
AbstractButton {
    id: btn
    property string iconName: "close"
    property color iconColor: Theme.textDim
    property bool danger: false
    property bool primary: false
    property bool bordered: false

    implicitWidth: Theme.touchTarget
    implicitHeight: Theme.touchTarget
    padding: 0

    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            name: btn.iconName
            size: 20
            color: btn.danger ? Theme.danger
                 : btn.primary ? Theme.onAccent
                 : (btn.down || btn.hovered ? Theme.text : btn.iconColor)
        }
    }

    background: Rectangle {
        radius: Theme.radius
        color: btn.primary
               ? (btn.down ? Theme.accentDim : Theme.accent)
               : (btn.down || btn.hovered ? Theme.surfaceHigh : "transparent")
        border.width: btn.bordered && !btn.primary ? 1 : 0
        border.color: Theme.outlineLight
    }
}
