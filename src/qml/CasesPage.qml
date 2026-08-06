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
            text: AppController.cases.length + " cases · "
                  + AppController.availableCells + " cellules disponibles"
            color: Theme.textDim
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }
        Label {
            Layout.fillWidth: true
            visible: AppController.gridsDirty
            text: "⚠ Les grilles ne reflètent plus les cases — regénérez-les."
            color: Theme.warning
            wrapMode: Text.WordWrap
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: addCaseCol.implicitHeight + 20
            radius: Theme.radiusLg
            color: Theme.surface
            border.color: Theme.outline
            ColumnLayout {
                id: addCaseCol
                anchors.fill: parent
                anchors.margins: Theme.pad
                spacing: 8
                Label { text: "Nouvelle phrase"; color: Theme.text; font.weight: Font.DemiBold }
                ColoTextField {
                    id: newLabel
                    Layout.fillWidth: true
                    hint: "Texte de la case"
                }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 8
                    rowSpacing: 8
                    Label { text: "Points"; color: Theme.textDim; font.pixelSize: 12 }
                    SpinBox { id: pts; from: 0; to: 99; value: 1; Layout.fillWidth: true }
                    Label { text: "Taux %"; color: Theme.textDim; font.pixelSize: 12 }
                    SpinBox {
                        id: rate
                        from: 0; to: 100; value: 50
                        Layout.fillWidth: true
                        textFromValue: function(value, locale) { return value + " %"; }
                    }
                }
                BingoButton {
                    Layout.fillWidth: true
                    text: "Ajouter"
                    primary: true
                    onClicked: {
                        if (newLabel.text.trim().length === 0) return
                        AppController.addCase(newLabel.text, pts.value, rate.value)
                        newLabel.text = ""
                    }
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
                Label { text: "Gages de combinaison"; color: Theme.text; font.weight: Font.DemiBold }
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

        Repeater {
            model: AppController.cases
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: caseRow.implicitHeight + 20
                radius: Theme.radius
                color: Theme.surface
                border.color: Theme.outline
                RowLayout {
                    id: caseRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: 10
                    spacing: 8
                    Label {
                        Layout.fillWidth: true
                        text: modelData.label
                        color: Theme.text
                        font.pixelSize: 14
                        wrapMode: Text.WordWrap
                        maximumLineCount: 3
                        elide: Text.ElideRight
                    }
                    Label {
                        text: modelData.points + " pt · " + modelData.rate + "%"
                        color: Theme.textDim
                        font.pixelSize: 12
                        Layout.maximumWidth: 96
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignRight
                    }
                    ToolButton {
                        text: "✕"
                        onClicked: AppController.removeCase(index)
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            visible: AppController.gageMode || AppController.gages.length > 0
            implicitHeight: gageCol.implicitHeight + 20
            radius: Theme.radiusLg
            color: Theme.surface
            border.color: Theme.outline
            ColumnLayout {
                id: gageCol
                anchors.fill: parent
                anchors.margins: Theme.pad
                spacing: 8
                Label { text: "Gages"; color: Theme.text; font.weight: Font.DemiBold }
                RowLayout {
                    Layout.fillWidth: true
                    ColoTextField {
                        id: gageDesc
                        Layout.fillWidth: true
                        hint: "Description du gage"
                    }
                    SpinBox { id: gageHp; from: 0; to: 100; value: 5 }
                    BingoButton {
                        text: "+"
                        primary: true
                        onClicked: AppController.addGage(gageDesc.text, gageHp.value)
                    }
                }
                Repeater {
                    model: AppController.gages
                    Label {
                        Layout.fillWidth: true
                        text: (index + 1) + ". " + modelData.description
                        color: Theme.text
                        wrapMode: Text.WordWrap
                        maximumLineCount: 4
                        elide: Text.ElideRight
                        font.pixelSize: 13
                    }
                }
            }
        }
    }
}
