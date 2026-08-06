import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    anchors.margins: Theme.pad
    spacing: Theme.gap

    Label {
        visible: AppController.gridsDirty
        text: "Cases modifiées depuis la dernière génération."
        color: Theme.warning
    }

    Button {
        text: "Générer toutes les grilles"
        onClicked: AppController.generateAll()
    }

    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        spacing: Theme.gap
        model: AppController.grids
        delegate: Rectangle {
            width: parent.width
            implicitHeight: innerCol.implicitHeight + 24
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.outline

            Column {
                id: innerCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 12
                spacing: 8
                Label { text: modelData.player; color: Theme.text; font.weight: Font.DemiBold }
                Column {
                    spacing: 4
                    Repeater {
                        model: modelData.cells
                        Row {
                            spacing: 4
                            Repeater {
                                model: modelData
                                Rectangle {
                                    width: 64; height: 48
                                    radius: 8
                                    color: modelData.isFree ? Theme.accentSoft : Theme.surfaceHigh
                                    border.color: Theme.outline
                                    Label {
                                        anchors.centerIn: parent
                                        width: parent.width - 4
                                        text: modelData.label
                                        font.pixelSize: 9
                                        wrapMode: Text.WordWrap
                                        horizontalAlignment: Text.AlignHCenter
                                        color: Theme.text
                                    }
                                }
                            }
                        }
                    }
                }
                Button { text: "Reshuffle"; onClicked: AppController.reshuffleGrid(index) }
            }
        }
    }
}
