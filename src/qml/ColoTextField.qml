import QtQuick
import QtQuick.Controls
import QtQuick.Window
import "scrollutils.js" as ScrollUtils

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
    clip: true
    maximumLength: 512

    background: Rectangle {
        radius: Theme.radius
        color: Theme.inputBg
        border.width: field.activeFocus ? 2 : 1
        border.color: field.activeFocus ? Theme.accent : Theme.outlineLight
        clip: true
    }

    onActiveFocusChanged: {
        if (activeFocus)
            Qt.callLater(function () { ScrollUtils.ensureVisible(field) })
    }

    // Clavier virtuel : re-scroller quand le viewport rétrécit.
    Connections {
        target: Qt.inputMethod
        function onKeyboardRectangleChanged() {
            if (field.activeFocus)
                Qt.callLater(function () { ScrollUtils.ensureVisible(field) })
        }
        function onVisibleChanged() {
            if (field.activeFocus && Qt.inputMethod.visible)
                Qt.callLater(function () { ScrollUtils.ensureVisible(field) })
        }
    }
}
