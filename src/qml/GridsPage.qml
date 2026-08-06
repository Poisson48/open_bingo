import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    clip: true
    contentWidth: availableWidth

    ColumnLayout {
        width: availableWidth - Theme.pad * 2
        anchors.margins: Theme.pad
        spacing: Theme.gap

        Label {
            Layout.fillWidth: true
            visible: AppController.gridsDirty
            text: "Cases modifiées — regénérez les grilles."
            color: Theme.warning
            wrapMode: Text.WordWrap
        }

        BingoButton {
            text: "Générer toutes les grilles"
            primary: true
            onClicked: AppController.generateAll()
        }

        Repeater {
            model: AppController.grids
            Rectangle {
                id: gridCard
                Layout.fillWidth: true
                implicitHeight: innerCol.implicitHeight + 24
                radius: Theme.radiusLg
                color: Theme.surface
                border.color: Theme.outline

                ColumnLayout {
                    id: innerCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: Theme.pad
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        Column {
                            spacing: 2
                            BingoButton {
                                text: "↑"
                                enabled: index > 0
                                onClicked: AppController.moveGrid(index, index - 1)
                            }
                            BingoButton {
                                text: "↓"
                                enabled: index < AppController.grids.length - 1
                                onClicked: AppController.moveGrid(index, index + 1)
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            text: modelData.player
                            color: Theme.text
                            font.weight: Font.DemiBold
                            font.pixelSize: 15
                            elide: Text.ElideRight
                        }
                        BingoButton {
                            text: "Reshuffle"
                            onClicked: AppController.reshuffleGrid(index)
                        }
                    }

                    BingoGrid {
                        Layout.fillWidth: true
                        availableWidth: gridCard.width - Theme.pad * 2
                        rows: modelData.cells
                        gageMode: AppController.gageMode
                    }
                }
            }
        }
    }
}
