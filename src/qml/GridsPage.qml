import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: page
    clip: true
    contentWidth: availableWidth

    // Forcer le rebind quand les grilles changent (swap / move / generate).
    readonly property int gridsRev: AppController.gridsRevision
    readonly property var gridList: {
        void gridsRev
        return AppController.grids
    }

    ColumnLayout {
        x: Theme.pad
        width: Math.max(0, page.availableWidth - Theme.pad * 2)
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
            visible: page.gridList.length > 0
            text: "Glisser une case pour l'échanger · Toucher pour remplacer une phrase · ↑↓ pour réordonner les joueurs."
            color: Theme.textDim
            wrapMode: Text.WordWrap
            font.pixelSize: 12
        }

        Label {
            Layout.fillWidth: true
            visible: page.gridList.length === 0 && AppController.cases.length > 0
            text: "Aucune grille — cliquez sur « Générer toutes les grilles »."
            color: Theme.textDim
            wrapMode: Text.WordWrap
            font.pixelSize: 13
        }

        Repeater {
            model: page.gridList
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
                            spacing: 4
                            BingoButton {
                                text: "Monter"
                                enabled: index > 0
                                onClicked: AppController.moveGrid(index, index - 1)
                            }
                            BingoButton {
                                text: "Descendre"
                                enabled: index < page.gridList.length - 1
                                onClicked: AppController.moveGrid(index, index + 1)
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            text: "#" + (index + 1) + " · " + modelData.player
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
                        editable: true
                        playerIndex: index
                        gageMode: AppController.gageMode
                        onCellEditRequested: function(r, c, label) {
                            cellPicker.openFor(index, r, c, label)
                        }
                    }
                }
            }
        }
    }

    CellPicker { id: cellPicker }
}
