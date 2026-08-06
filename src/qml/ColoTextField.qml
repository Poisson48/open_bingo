import QtQuick
import QtQuick.Controls

TextField {
    id: field

    property string hint: ""
    placeholderText: (activeFocus || length > 0) ? "" : hint

    implicitHeight: 44
    leftPadding: 12
    rightPadding: 12
    color: Theme.text
    placeholderTextColor: Theme.textDim
    font.pixelSize: 14
    selectByMouse: true

    background: Rectangle {
        radius: Theme.radius
        color: Theme.inputBg
        border.width: field.activeFocus ? 2 : 1
        border.color: field.activeFocus ? Theme.accent : Theme.outlineLight
    }
}
