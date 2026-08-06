import QtQuick
import QtQuick.Controls

Item {
    id: root
    property int currentIndex: 0
    property var labels: []
    implicitHeight: 44

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

    ScrollView {
        anchors.fill: parent
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AsNeeded
        ScrollBar.vertical.policy: ScrollBar.AlwaysOff

        Row {
            spacing: 2
            height: root.height

            Repeater {
                model: tabModel
                TabButton {
                    width: Math.max(72, tabLabel.implicitWidth + 24)
                    height: parent.height
                    checked: root.currentIndex === index
                    onClicked: root.currentIndex = index

                    contentItem: Label {
                        id: tabLabel
                        text: model.label
                        color: parent.checked ? Theme.accent : Theme.textDim
                        font.pixelSize: 13
                        font.weight: parent.checked ? Font.DemiBold : Font.Normal
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }

                    background: Item {
                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 3
                            radius: 2
                            color: parent.parent.checked ? Theme.accent : "transparent"
                        }
                    }
                }
            }
        }
    }
}
