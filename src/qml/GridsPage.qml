import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    clip: true
    contentWidth: availableWidth

    ColumnLayout {
        x: Theme.pad
        width: Math.max(0, availableWidth - Theme.pad * 2)
        spacing: Theme.gap

        Label {
            Layout.fillWidth: true
            visible: AppController.gridsDirty
            text: "Cases modifiées — regénérez les grilles."
            color: Theme.warning
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            visible: AppController.cases.length === 0
            text: "Ajoutez des phrases dans l'onglet Phrases avant de générer."
            color: Theme.textDim
            wrapMode: Text.WordWrap
            font.pixelSize: 13
        }

        BingoButton {
            Layout.fillWidth: true
            text: "Générer toutes les grilles"
            primary: true
            onClicked: AppController.generateAll()
        }

        Label {
            Layout.fillWidth: true
            visible: AppController.grids.length === 0 && AppController.cases.length > 0
            text: "Aucune grille — cliquez sur « Générer toutes les grilles »."
            color: Theme.textDim
            wrapMode: Text.WordWrap
            font.pixelSize: 13
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
                        spacing: 6
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
                            maximumLineCount: 1
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
