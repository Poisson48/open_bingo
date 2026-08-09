import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    clip: true
    contentWidth: availableWidth

    ColumnLayout {
        x: Theme.pad
        width: Math.min(Math.max(0, availableWidth - Theme.pad * 2), Theme.contentMax)
        spacing: Theme.gap

        Label {
            Layout.fillWidth: true
            text: AppController.gageMode
                  ? "Mode gage : chaque phrase (onglet Phrases) pointe un n° de ce tableau. Plusieurs gages peuvent partager le même n° — au cochage, tirage pondéré selon la chance % (ex. 90 / 10)."
                  : "Mode classique : gages optionnels pour récupérer des PV. Active « Mode gage » dans Réglages pour lier les phrases."
            color: Theme.textDim
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: addCol.implicitHeight + 20
            radius: Theme.radiusLg
            color: Theme.surface
            border.color: Theme.outline
            ColumnLayout {
                id: addCol
                anchors.fill: parent
                anchors.margins: Theme.pad
                spacing: 8

                Label {
                    text: "Nouveau gage"
                    color: Theme.text
                    font.weight: Font.DemiBold
                }
                ColoTextField {
                    id: gageDesc
                    Layout.fillWidth: true
                    hint: "Description (ex. boire une gorgée)"
                    onAccepted: addGageBtn.clicked()
                }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 8
                    rowSpacing: 8
                    Label {
                        text: "N°"
                        color: Theme.textDim
                        font.pixelSize: 12
                    }
                    ColoSpinBox {
                        id: gageNumber
                        from: 1; to: 99
                        value: Math.max(1, AppController.maxGageNumber())
                        Layout.fillWidth: true
                    }
                    Label {
                        text: "Chance relative %"
                        color: Theme.textDim
                        font.pixelSize: 12
                    }
                    ColoSpinBox {
                        id: gageRate
                        from: 0; to: 100; value: 100
                        Layout.fillWidth: true
                        textFromValue: function(value, locale) { return value + " %"; }
                    }
                    Label {
                        visible: !AppController.gageMode
                        text: "PV récupérés"
                        color: Theme.textDim
                        font.pixelSize: 12
                    }
                    ColoSpinBox {
                        id: gageHp
                        visible: !AppController.gageMode
                        from: 0; to: 100; value: 5
                        Layout.fillWidth: true
                    }
                }
                Label {
                    Layout.fillWidth: true
                    visible: AppController.gageMode
                    text: "Même n° qu’un gage existant = variante (risquée à faible %). Monte le n° pour un nouveau slot."
                    color: Theme.textDim
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
                BingoButton {
                    id: addGageBtn
                    Layout.fillWidth: true
                    text: "Ajouter le gage"
                    primary: true
                    onClicked: {
                        if (gageDesc.text.trim().length === 0) {
                            AppController.notify("Écrivez la description du gage")
                            return
                        }
                        AppController.addGage(gageDesc.text,
                                              AppController.gageMode ? 0 : gageHp.value,
                                              gageNumber.value,
                                              gageRate.value)
                        gageDesc.text = ""
                        gageNumber.value = Math.max(1, AppController.maxGageNumber())
                        gageRate.value = 100
                        gageDesc.forceActiveFocus()
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: AppController.gages.length === 0
            text: "Aucun gage — ajoutez-en au moins un ci-dessus."
            color: Theme.warning
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            visible: AppController.gages.length > 0
            text: AppController.gages.length + " gage(s) — touchez pour modifier n°, chance % ou texte."
            color: Theme.textDim
            font.pixelSize: 12
        }

        Repeater {
            model: AppController.gages
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: gageRow.implicitHeight + 16
                radius: Theme.radius
                color: Theme.surface
                border.color: Theme.outline
                RowLayout {
                    id: gageRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: 10
                    spacing: 8
                    Label {
                        text: "#" + (modelData.number || (index + 1))
                                + " · " + (modelData.rate !== undefined ? modelData.rate : 100) + "%"
                        color: Theme.accent
                        font.weight: Font.DemiBold
                        font.pixelSize: 13
                        Layout.preferredWidth: 72
                    }
                    Label {
                        Layout.fillWidth: true
                        text: {
                            const d = modelData.description || ""
                            if (AppController.gageMode || !modelData.hp)
                                return d
                            return d + "  (+" + modelData.hp + " PV)"
                        }
                        color: Theme.text
                        wrapMode: Text.WordWrap
                        maximumLineCount: 5
                        elide: Text.ElideRight
                        font.pixelSize: 14
                    }
                    IconButton {
                        iconName: "trash"
                        danger: true
                        onClicked: AppController.removeGage(index)
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    anchors.rightMargin: 48
                    onClicked: editGageDialog.openFor(index, modelData)
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            visible: AppController.gageMode
            implicitHeight: comboCol.implicitHeight + 20
            radius: Theme.radiusLg
            color: Theme.surface
            border.color: Theme.outline
            ColumnLayout {
                id: comboCol
                anchors.fill: parent
                anchors.margins: Theme.pad
                spacing: 8
                Label {
                    text: "Gages de combinaison"
                    color: Theme.text
                    font.weight: Font.DemiBold
                }
                Label {
                    Layout.fillWidth: true
                    text: "Déclenchés quand une ligne, colonne ou diagonale est complète."
                    color: Theme.textDim
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
                ColoTextField {
                    Layout.fillWidth: true
                    text: AppController.comboGages.line
                    hint: "Gage — ligne complète"
                    onEditingFinished: AppController.setComboGage("line", text)
                }
                ColoTextField {
                    Layout.fillWidth: true
                    text: AppController.comboGages.column
                    hint: "Gage — colonne complète"
                    onEditingFinished: AppController.setComboGage("column", text)
                }
                ColoTextField {
                    Layout.fillWidth: true
                    text: AppController.comboGages.diagonal
                    hint: "Gage — diagonale complète"
                    onEditingFinished: AppController.setComboGage("diagonal", text)
                }
            }
        }

        ColoDialog {
            id: editGageDialog
            title: "Modifier le gage"
            acceptText: "Enregistrer"
            acceptEnabled: editGageDesc.text.trim().length > 0
            property int gageIndex: -1

            function openFor(idx, data) {
                gageIndex = idx
                editGageDesc.text = data.description || ""
                editGageHp.value = data.hp !== undefined ? data.hp : 5
                editGageNumber.value = data.number !== undefined ? data.number : (idx + 1)
                editGageRate.value = data.rate !== undefined ? data.rate : 100
                open()
                editGageDesc.forceActiveFocus()
            }

            ColoTextField {
                id: editGageDesc
                Layout.fillWidth: true
                hint: "Description du gage"
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 8
                rowSpacing: 8
                Label {
                    text: "N°"
                    color: Theme.textDim
                    font.pixelSize: 12
                }
                ColoSpinBox {
                    id: editGageNumber
                    from: 1; to: 99
                    Layout.fillWidth: true
                }
                Label {
                    text: "Chance relative %"
                    color: Theme.textDim
                    font.pixelSize: 12
                }
                ColoSpinBox {
                    id: editGageRate
                    from: 0; to: 100
                    Layout.fillWidth: true
                    textFromValue: function(value, locale) { return value + " %"; }
                }
                Label {
                    visible: !AppController.gageMode
                    text: "PV récupérés"
                    color: Theme.textDim
                    font.pixelSize: 12
                }
                ColoSpinBox {
                    id: editGageHp
                    visible: !AppController.gageMode
                    from: 0; to: 100
                    Layout.fillWidth: true
                }
            }

            onAccepted: {
                if (gageIndex < 0) return
                AppController.updateGage(gageIndex, editGageDesc.text.trim(),
                                         AppController.gageMode ? 0 : editGageHp.value,
                                         editGageNumber.value, editGageRate.value)
            }
        }

        Item { Layout.preferredHeight: Theme.pad * 2 }
    }
}
