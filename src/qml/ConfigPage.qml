import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    clip: true
    contentWidth: availableWidth

    ColumnLayout {
        width: Math.min(availableWidth - Theme.pad * 2, Theme.contentMax)
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: Theme.gap
        anchors.margins: Theme.pad

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: section.implicitHeight + 24
            radius: Theme.radiusLg
            color: Theme.surface
            border.color: Theme.outline

            ColumnLayout {
                id: section
                anchors.fill: parent
                anchors.margins: Theme.pad
                spacing: Theme.gap

                Label { text: "Projet"; color: Theme.text; font.weight: Font.DemiBold; font.pixelSize: 14 }
                ColoTextField {
                    Layout.fillWidth: true
                    text: AppController.title
                    hint: "Titre"
                    onEditingFinished: AppController.title = text
                }
                ColoTextField {
                    Layout.fillWidth: true
                    text: AppController.description
                    hint: "Description (optionnel)"
                    onEditingFinished: AppController.description = text
                }

                Label { text: "Grille"; color: Theme.text; font.weight: Font.DemiBold; font.pixelSize: 14 }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "Taille"; color: Theme.textDim; Layout.fillWidth: true }
                    SpinBox {
                        from: 2; to: 12; value: AppController.gridSize
                        onValueModified: AppController.gridSize = value
                    }
                }

                CheckBox {
                    text: "Case FREE au centre"
                    checked: AppController.freeCenter
                    onCheckedChanged: AppController.freeCenter = checked
                }
                CheckBox {
                    text: "Mode gage"
                    checked: AppController.gageMode
                    onCheckedChanged: AppController.gageMode = checked
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: !AppController.gageMode
                    Label { text: "HP de départ"; color: Theme.textDim; Layout.fillWidth: true }
                    SpinBox {
                        from: 1; to: 100; value: AppController.startHP
                        onValueModified: AppController.startHP = value
                    }
                }

                Label {
                    text: "Multiplicateurs de points"
                    visible: !AppController.gageMode
                    color: Theme.text
                    font.weight: Font.DemiBold
                    font.pixelSize: 14
                }
                GridLayout {
                    Layout.fillWidth: true
                    visible: !AppController.gageMode
                    columns: 2
                    rowSpacing: 8
                    columnSpacing: 12
                    Label { text: "Ligne"; color: Theme.textDim }
                    SpinBox {
                        from: 1; to: 99; value: AppController.multipliers.line
                        onValueModified: AppController.setMultiplier("line", value)
                    }
                    Label { text: "Colonne"; color: Theme.textDim }
                    SpinBox {
                        from: 1; to: 99; value: AppController.multipliers.column
                        onValueModified: AppController.setMultiplier("column", value)
                    }
                    Label { text: "Diagonale"; color: Theme.textDim }
                    SpinBox {
                        from: 1; to: 99; value: AppController.multipliers.diagonal
                        onValueModified: AppController.setMultiplier("diagonal", value)
                    }
                    Label { text: "Grille complète"; color: Theme.textDim }
                    SpinBox {
                        from: 1; to: 99; value: AppController.multipliers.full
                        onValueModified: AppController.setMultiplier("full", value)
                    }
                }

                Label { text: "Joueurs"; color: Theme.text; font.weight: Font.DemiBold; font.pixelSize: 14 }
                Repeater {
                    model: AppController.players
                    RowLayout {
                        Layout.fillWidth: true
                        Rectangle {
                            Layout.preferredWidth: 22
                            Layout.preferredHeight: 22
                            radius: 11
                            color: Theme.accentSoft
                            Label {
                                anchors.centerIn: parent
                                text: index + 1
                                color: Theme.accent
                                font.pixelSize: 11
                                font.weight: Font.Bold
                            }
                        }
                        ColoTextField {
                            Layout.fillWidth: true
                            text: modelData.name
                            hint: "Nom du joueur"
                            onEditingFinished: AppController.setPlayerName(index, text)
                        }
                        ToolButton {
                            text: "✕"
                            onClicked: AppController.removePlayer(index)
                        }
                    }
                }
                BingoButton {
                    text: "+ Ajouter un joueur"
                    onClicked: AppController.addPlayer()
                }

                BingoButton {
                    Layout.fillWidth: true
                    text: "Enregistrer la configuration"
                    primary: true
                    onClicked: AppController.saveConfig()
                }
            }
        }
    }
}
