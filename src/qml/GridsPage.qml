import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: page
    clip: true
    contentWidth: availableWidth

    // Forcer le rebind quand les grilles changent (swap / assign / generate).
    readonly property int gridsRev: AppController.gridsRevision
    readonly property var gridList: {
        void gridsRev
        return AppController.grids
    }
    readonly property var playerNames: {
        void AppController.currentProjectId
        const list = AppController.players
        var names = []
        for (var i = 0; i < list.length; ++i)
            names.push(list[i].name)
        return names
    }

    ColumnLayout {
        x: Theme.pad
        width: Math.min(Math.max(0, page.availableWidth - Theme.pad * 2), Theme.contentMax)
        spacing: Theme.gap

        Label {
            Layout.fillWidth: true
            visible: AppController.gridsDirty
            text: "Cases modifiées — regénère les grilles."
            color: Theme.warning
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            visible: AppController.cases.length === 0
            text: "Ajoute des phrases dans l'onglet Phrases avant de générer."
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
            text: "Assigne chaque grille à un joueur. Glisser une case pour l'échanger, toucher pour éditer."
            color: Theme.textDim
            wrapMode: Text.WordWrap
            font.pixelSize: 12
        }

        Label {
            Layout.fillWidth: true
            visible: page.gridList.length === 0 && AppController.cases.length > 0
            text: "Aucune grille — appuie sur « Générer toutes les grilles »."
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
                        spacing: 8

                        Label {
                            text: "#" + (index + 1)
                            color: Theme.textDim
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }

                        Label {
                            text: "Joueur"
                            color: Theme.textDim
                            font.pixelSize: 12
                        }

                        ColoComboBox {
                            id: playerCombo
                            Layout.fillWidth: true
                            model: {
                                // Inclure le nom actuel s'il n'est plus dans la liste joueurs.
                                var names = page.playerNames.slice()
                                const cur = modelData.player || ""
                                if (cur.length && names.indexOf(cur) < 0)
                                    names.push(cur)
                                return names
                            }
                            currentIndex: {
                                const cur = modelData.player || ""
                                const i = model.indexOf(cur)
                                return i >= 0 ? i : 0
                            }
                            onActivated: function(i) {
                                const name = model[i]
                                if (!name || name === modelData.player)
                                    return
                                AppController.assignGridToPlayer(index, name)
                            }
                        }

                        BingoButton {
                            text: "Mélanger"
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
                            const cell = modelData.cells[r][c]
                            cellPicker.openFor(index, r, c, label,
                                               cell && cell.points !== undefined ? cell.points : 1)
                        }
                    }
                }
            }
        }

        Item { Layout.preferredHeight: Theme.pad * 2 }
    }

    CellPicker { id: cellPicker }
}
