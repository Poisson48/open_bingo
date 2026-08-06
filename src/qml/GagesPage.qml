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
            text: AppController.gageMode
                  ? "Mode gage : chaque phrase (onglet Phrases) pointe un n° de ce tableau."
                  : "Mode classique : gages optionnels pour récupérer des PV. Activez « Mode gage » dans Config pour lier les phrases."
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
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    visible: !AppController.gageMode
                    Label {
                        text: "PV récupérés"
                        color: Theme.textDim
                        font.pixelSize: 12
                    }
                    SpinBox {
                        id: gageHp
                        from: 0; to: 100; value: 5
                        Layout.preferredWidth: 120
                    }
                    Item { Layout.fillWidth: true }
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
                                              AppController.gageMode ? 0 : gageHp.value)
                        gageDesc.text = ""
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
            text: AppController.gages.length + " gage(s) — touchez pour modifier."
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
                        text: "#" + (index + 1)
                        color: Theme.accent
                        font.weight: Font.DemiBold
                        font.pixelSize: 14
                        Layout.preferredWidth: 40
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
                open()
                editGageDesc.forceActiveFocus()
            }

            ColoTextField {
                id: editGageDesc
                Layout.fillWidth: true
                hint: "Description du gage"
            }
            RowLayout {
                Layout.fillWidth: true
                visible: !AppController.gageMode
                Label { text: "PV récupérés"; color: Theme.textDim; font.pixelSize: 12 }
                SpinBox { id: editGageHp; from: 0; to: 100; Layout.fillWidth: true }
            }

            onAccepted: {
                if (gageIndex < 0) return
                AppController.updateGage(gageIndex, editGageDesc.text.trim(),
                                         AppController.gageMode ? 0 : editGageHp.value)
            }
        }

        Item { Layout.preferredHeight: Theme.pad * 2 }
    }
}
