import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

ScrollView {
    id: casesScroll
    clip: true
    contentWidth: availableWidth

    ColumnLayout {
        x: Theme.pad
        width: Math.min(Math.max(0, availableWidth - Theme.pad * 2), Theme.contentMax)
        spacing: Theme.gap

        BingoButton {
            Layout.fillWidth: true
            visible: Qt.platform.os !== "android"
            text: "Depuis un film…"
            primary: true
            onClicked: {
                const w = casesScroll.Window.window
                if (w && typeof w.pushFilmAssist === "function")
                    w.pushFilmAssist()
            }
        }
        Label {
            Layout.fillWidth: true
            visible: Qt.platform.os !== "android"
            text: "Prépare des phrases à partir des sous-titres malentendants."
            color: Theme.textDim
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            text: AppController.cases.length + " phrases · "
                  + AppController.availableCells + " cellules disponibles"
            color: Theme.textDim
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }
        Label {
            Layout.fillWidth: true
            visible: AppController.gridsDirty
            text: "Les grilles ne reflètent plus les cases — regénère-les."
            color: Theme.warning
            wrapMode: Text.WordWrap
        }
        Label {
            Layout.fillWidth: true
            visible: AppController.gageMode
            text: AppController.gages.length > 0
                  ? ("Mode gage : « N° gage » = n° de l’onglet Gages (1–"
                     + AppController.maxGageNumber()
                     + "). Plusieurs gages du même n° → tirage pondéré au cochage.")
                  : "Mode gage : créez d’abord des gages dans l’onglet Gages."
            color: AppController.gages.length > 0 ? Theme.textDim : Theme.warning
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap
            BingoButton {
                Layout.fillWidth: true
                text: "Importer CSV"
                onClicked: importCsvDialog.open()
            }
            BingoButton {
                Layout.fillWidth: true
                text: "Exporter CSV"
                onClicked: AppController.pickExportPhrasesCsv()
            }
        }
        Label {
            Layout.fillWidth: true
            text: "Tu peux aussi préparer un tableur CSV (label, points, rate)."
            color: Theme.textDim
            font.pixelSize: 12
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
                    Label {
                        text: AppController.gageMode ? "N° gage" : "Points"
                        color: Theme.textDim
                        font.pixelSize: 12
                    }
                    ColoSpinBox {
                        id: pts
                        from: AppController.gageMode ? 1 : 0
                        to: 99
                        value: 1
                        Layout.fillWidth: true
                    }
                    Label { text: "Taux %"; color: Theme.textDim; font.pixelSize: 12 }
                    ColoSpinBox {
                        id: rate
                        from: 0; to: 100; value: 50
                        Layout.fillWidth: true
                        textFromValue: function(value, locale) { return value + " %"; }
                    }
                }
                BingoButton {
                    Layout.fillWidth: true
                    text: "Ajouter la phrase"
                    primary: true
                    onClicked: {
                        if (newLabel.text.trim().length === 0) return
                        AppController.addCase(newLabel.text, pts.value, rate.value)
                        newLabel.text = ""
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: AppController.cases.length > 0
            text: AppController.gageMode
                  ? "Touche une phrase pour changer le texte, le n° de gage ou le taux."
                  : "Touche une phrase pour modifier son texte, ses points ou son taux."
            color: Theme.textDim
            font.pixelSize: 12
            wrapMode: Text.WordWrap
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
                        text: AppController.gageMode
                              ? ("#" + modelData.points + " · " + modelData.rate + "%")
                              : (modelData.points + " pt · " + modelData.rate + "%")
                        color: AppController.gageMode ? Theme.accent : Theme.textDim
                        font.pixelSize: 12
                        Layout.maximumWidth: 96
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignRight
                    }
                    IconButton {
                        iconName: "trash"
                        danger: true
                        onClicked: AppController.removeCase(index)
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    anchors.rightMargin: 48
                    onClicked: editCaseDialog.openFor(index, modelData)
                }
            }
        }

        ColoDialog {
            id: importCsvDialog
            title: "Importer les phrases"
            showAccept: false

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textDim
                font.pixelSize: 14
                text: "Remplacer la liste actuelle, ou ajouter les lignes du CSV à la suite ?"
            }
            BingoButton {
                Layout.fillWidth: true
                text: "Ajouter"
                primary: true
                onClicked: {
                    importCsvDialog.close()
                    AppController.pickImportPhrasesCsv(false)
                }
            }
            BingoButton {
                Layout.fillWidth: true
                text: "Remplacer"
                danger: true
                onClicked: {
                    importCsvDialog.close()
                    AppController.pickImportPhrasesCsv(true)
                }
            }
        }

        ColoDialog {
            id: editCaseDialog
            title: "Modifier la phrase"
            acceptText: "Enregistrer"
            acceptEnabled: editLabel.text.trim().length > 0
            property int caseIndex: -1

            function openFor(idx, data) {
                caseIndex = idx
                editLabel.text = data.label || ""
                editPts.value = data.points !== undefined ? data.points : 1
                editRate.value = data.rate !== undefined ? data.rate : 50
                open()
                editLabel.forceActiveFocus()
            }

            ColoTextField {
                id: editLabel
                Layout.fillWidth: true
                hint: "Texte de la case"
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 8
                rowSpacing: 8
                Label {
                    text: AppController.gageMode ? "N° gage" : "Points"
                    color: Theme.textDim
                    font.pixelSize: 12
                }
                ColoSpinBox {
                    id: editPts
                    from: AppController.gageMode ? 1 : 0
                    to: 99
                    Layout.fillWidth: true
                }
                Label { text: "Taux %"; color: Theme.textDim; font.pixelSize: 12 }
                ColoSpinBox {
                    id: editRate
                    from: 0; to: 100
                    Layout.fillWidth: true
                    textFromValue: function(value, locale) { return value + " %"; }
                }
            }

            onAccepted: {
                if (caseIndex < 0) return
                AppController.updateCase(caseIndex, editLabel.text.trim(),
                                         editPts.value, editRate.value)
            }
        }

        Item { Layout.preferredHeight: Theme.pad * 2 }
    }
}
