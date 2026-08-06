import QtQuick
import QtQuick.Controls

Item {
    id: root
    property int currentIndex: 0
    property var labels: []
    implicitHeight: 48

    ListModel {
        id: tabModel
        Component.onCompleted: rebuild()
    }

    function rebuild() {
        tabModel.clear()
        for (var i = 0; i < labels.length; ++i)
            tabModel.append({ "label": labels[i] })
    }

    onLabelsChanged: rebuild()

    Rectangle {
        anchors.fill: parent
        color: Theme.surface
    }

    Flickable {
        anchors.fill: parent
        contentWidth: Math.max(width, tabRow.implicitWidth)
        contentHeight: height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.HorizontalFlick

        Row {
            id: tabRow
            spacing: 0
            height: root.height

            Repeater {
                model: tabModel
                AbstractButton {
                    width: Math.max(76, tabLabel.implicitWidth + 28)
                    height: parent.height
                    onClicked: root.currentIndex = index

                    contentItem: Label {
                        id: tabLabel
                        text: model.label
                        color: root.currentIndex === index ? Theme.accent : Theme.textDim
                        font.pixelSize: 13
                        font.weight: root.currentIndex === index ? Font.DemiBold : Font.Normal
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Item {
                        Rectangle {
                            anchors.bottom: parent.bottom
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: parent.width - 12
                            height: 3
                            radius: 2
                            color: root.currentIndex === index ? Theme.accent : "transparent"
                        }
                    }
                }
            }
        }
    }
}
