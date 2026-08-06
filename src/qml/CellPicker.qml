import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Remplacer une case de grille par une phrase du pool (comme l'ancienne app web).
Popup {
    id: picker
    property int playerIdx: -1
    property int cellRow: -1
    property int cellCol: -1
    property string currentLabel: ""

    parent: Overlay.overlay
    modal: true
    padding: 16
    width: Math.min(Overlay.overlay.width - 32, 420)
    height: Math.min(Overlay.overlay.height - 48, 520)
    x: (Overlay.overlay.width - width) / 2
    y: (Overlay.overlay.height - height) / 2
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.55) }

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusLg
        border.color: Theme.outline
    }

    function openFor(pIdx, r, c, label) {
        playerIdx = pIdx
        cellRow = r
        cellCol = c
        currentLabel = label || ""
        filterField.text = ""
        open()
        filterField.forceActiveFocus()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.gap

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: "Remplacer la case"
                color: Theme.text
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }
            ToolButton {
                text: "✕"
                onClicked: picker.close()
            }
        }

        Label {
            Layout.fillWidth: true
            visible: currentLabel.length > 0
            text: "Actuelle : " + currentLabel
            color: Theme.textDim
            font.pixelSize: 13
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
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
