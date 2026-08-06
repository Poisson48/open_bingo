import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    clip: true
    contentWidth: availableWidth

    ColumnLayout {
        x: Theme.pad
        width: Math.min(Math.max(0, availableWidth - Theme.pad * 2), Theme.contentMax)
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: Theme.gap

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
                // TextArea : une ligne unique tronquait le début de la description.
                TextArea {
                    id: descField
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(72, contentHeight + topPadding + bottomPadding)
                    text: AppController.description
                    placeholderText: activeFocus || length > 0 ? "" : "Description (optionnel)"
                    color: Theme.text
                    placeholderTextColor: Theme.textDim
                    font.pixelSize: 14
                    wrapMode: TextEdit.Wrap
                    selectByMouse: true
                    leftPadding: 12
                    rightPadding: 12
                    topPadding: 10
                    bottomPadding: 10
                    background: Rectangle {
                        radius: Theme.radius
                        color: Theme.inputBg
                        border.width: descField.activeFocus ? 2 : 1
                        border.color: descField.activeFocus ? Theme.accent : Theme.outlineLight
                    }
                    onActiveFocusChanged: {
                        if (!activeFocus)
                            AppController.description = text
                    }
                }

                Label { text: "Grille"; color: Theme.text; font.weight: Font.DemiBold; font.pixelSize: 14 }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "Taille"; color: Theme.textDim; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    SpinBox {
                        from: 2; to: 12
                        value: AppController.gridSize
                        onValueModified: AppController.gridSize = value
                    }
                }

                CheckBox {
                    Layout.fillWidth: true
                    text: "Case FREE au centre"
                    checked: AppController.freeCenter
                    onClicked: AppController.freeCenter = checked
                }
                CheckBox {
                    Layout.fillWidth: true
                    text: "Mode gage"
                    checked: AppController.gageMode
                    onClicked: AppController.gageMode = checked
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: !AppController.gageMode
                    Label { text: "HP de départ"; color: Theme.textDim; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    SpinBox {
                        from: 1; to: 100
                        value: AppController.startHP
                        onValueModified: AppController.startHP = value
                    }
                }

                Label {
                    text: "Multiplicateurs de points"
                    visible: !AppController.gageMode
                    color: Theme.text
                    font.weight: Font.DemiBold
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                GridLayout {
                    Layout.fillWidth: true
                    visible: !AppController.gageMode
                    columns: 2
                    rowSpacing: 8
                    columnSpacing: 12
                    Label { text: "Ligne"; color: Theme.textDim; Layout.fillWidth: true; elide: Text.ElideRight }
                    SpinBox {
                        from: 1; to: 99
                        value: AppController.multipliers.line
                        onValueModified: AppController.setMultiplier("line", value)
                    }
                    Label { text: "Colonne"; color: Theme.textDim; Layout.fillWidth: true; elide: Text.ElideRight }
                    SpinBox {
                        from: 1; to: 99
                        value: AppController.multipliers.column
                        onValueModified: AppController.setMultiplier("column", value)
                    }
                    Label { text: "Diagonale"; color: Theme.textDim; Layout.fillWidth: true; elide: Text.ElideRight }
                    SpinBox {
                        from: 1; to: 99
                        value: AppController.multipliers.diagonal
                        onValueModified: AppController.setMultiplier("diagonal", value)
                    }
                    Label { text: "Grille complète"; color: Theme.textDim; Layout.fillWidth: true; elide: Text.ElideRight }
                    SpinBox {
                        from: 1; to: 99
                        value: AppController.multipliers.full
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
                        IconButton {
                            iconName: "trash"
                            danger: true
                            onClicked: AppController.removePlayer(index)
                        }
                    }
                }
                BingoButton {
                    Layout.fillWidth: true
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

        Item { Layout.preferredHeight: Theme.pad * 2 }
    }
}
