import QtQuick
import QtQuick.Controls

ComboBox {
    id: control

    implicitHeight: Theme.touchTarget
    leftPadding: 12
    rightPadding: 36
    font.pixelSize: 14

    contentItem: Text {
        leftPadding: control.leftPadding
        rightPadding: control.indicator.width + control.spacing
        text: control.displayText
        font: control.font
        color: control.enabled ? Theme.text : Theme.textDim
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.radius
        color: Theme.inputBg
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.accent : Theme.outlineLight
    }

    popup: Popup {
        y: control.height + 4
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 280)
        padding: 4
        background: Rectangle {
            color: Theme.surface
            radius: Theme.radiusLg
            border.color: Theme.outline
            border.width: 1
        }
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator { }
        }
    }

    delegate: ItemDelegate {
        width: control.width
        implicitHeight: Theme.touchTarget
        highlighted: control.highlightedIndex === index
        contentItem: Text {
            text: control.textRole
                  ? (control.model && control.model[index]
                     ? (control.model[index][control.textRole]
                        !== undefined ? control.model[index][control.textRole]
                        : modelData)
                     : modelData)
                  : modelData
            color: Theme.text
            font.pixelSize: 14
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            color: parent.highlighted ? Theme.surfaceHigh : "transparent"
            radius: Theme.radius
        }
    }
}
