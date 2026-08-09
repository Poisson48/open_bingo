import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Remplacer / éditer une case de grille (pool ou texte libre).
Popup {
    id: picker
    property int playerIdx: -1
    property int cellRow: -1
    property int cellCol: -1
    property string currentLabel: ""
    property int currentPoints: 1

    parent: Overlay.overlay
    modal: true
    padding: 16
    width: Math.min(Overlay.overlay.width - 32, 420)
    height: Math.min(Overlay.overlay.height - 48, 560)
    x: (Overlay.overlay.width - width) / 2
    y: (Overlay.overlay.height - height) / 2
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.55) }

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusLg
        border.color: Theme.outline
    }

    function openFor(pIdx, r, c, label, points) {
        playerIdx = pIdx
        cellRow = r
        cellCol = c
        currentLabel = label || ""
        currentPoints = (points !== undefined && points >= 0) ? points : 1
        editLabel.text = currentLabel
        editPts.value = currentPoints
        filterField.text = ""
        open()
        editLabel.forceActiveFocus()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.gap

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: "Modifier la case"
                color: Theme.text
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }
            IconButton {
                iconName: "close"
                onClicked: picker.close()
            }
        }

        Label {
            Layout.fillWidth: true
            text: "Texte libre"
            color: Theme.textDim
            font.pixelSize: 12
        }

        ColoTextField {
            id: editLabel
            Layout.fillWidth: true
            hint: "Texte de la case"
        }

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: AppController.gageMode ? "N° gage" : "Points"
                color: Theme.textDim
                font.pixelSize: 12
            }
            ColoSpinBox {
                id: editPts
                from: 0; to: 99
                Layout.fillWidth: true
            }
            BingoButton {
                text: "Enregistrer"
                primary: true
                enabled: editLabel.text.trim().length > 0
                onClicked: {
                    AppController.setGridCellLabel(
                        picker.playerIdx, picker.cellRow, picker.cellCol,
                        editLabel.text, editPts.value)
                    picker.close()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.outline
        }

        Label {
            Layout.fillWidth: true
            text: "Ou choisir une phrase du pool"
            color: Theme.textDim
            font.pixelSize: 12
        }

        ColoTextField {
            id: filterField
            Layout.fillWidth: true
            hint: "Filtrer les phrases…"
        }

        ListView {
            id: caseList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 4
            model: AppController.cases

            delegate: Item {
                width: caseList.width
                height: matches ? row.implicitHeight + 12 : 0
                visible: matches
                property bool matches: {
                    const q = filterField.text.trim().toLowerCase()
                    if (!q.length) return true
                    return modelData.label.toLowerCase().indexOf(q) >= 0
                }

                Rectangle {
                    id: row
                    width: parent.width
                    implicitHeight: lab.implicitHeight + 16
                    radius: Theme.radius
                    color: modelData.label === picker.currentLabel
                           ? Theme.accentSoft : Theme.inputBg
                    border.color: modelData.label === picker.currentLabel
                                  ? Theme.accent : Theme.outline

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8
                        Label {
                            id: lab
                            Layout.fillWidth: true
                            text: modelData.label
                            color: Theme.text
                            wrapMode: Text.WordWrap
                            maximumLineCount: 3
                            elide: Text.ElideRight
                            font.pixelSize: 13
                        }
                        Label {
                            text: modelData.points + " pt"
                            color: Theme.textDim
                            font.pixelSize: 11
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            AppController.replaceGridCell(
                                picker.playerIdx, picker.cellRow, picker.cellCol, index)
                            picker.close()
                        }
                    }
                }
            }
        }
    }
}
