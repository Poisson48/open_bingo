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
            text: "Aperçu A4 — 2 grilles par page (nom, points / n° de gage, PV ou note, + feuille des gages). L’impression ouvre le service système ; tu peux aussi enregistrer un PDF."
            color: Theme.textDim
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            visible: AppController.grids.length === 0
            text: "Aucune grille à imprimer — génère-les d’abord dans l’onglet Grilles."
            color: Theme.warning
            wrapMode: Text.WordWrap
            font.pixelSize: 13
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap
            BingoButton {
                Layout.fillWidth: true
                text: "Imprimer…"
                primary: true
                enabled: AppController.grids.length > 0
                onClicked: AppController.printGrids()
            }
            BingoButton {
                Layout.fillWidth: true
                text: "Enregistrer PDF"
                enabled: AppController.grids.length > 0
                onClicked: AppController.savePdf()
            }
        }

        Repeater {
            model: AppController.grids
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: inner.implicitHeight + 24
                color: Theme.surface
                border.color: Theme.outline
                radius: Theme.radiusLg

                ColumnLayout {
                    id: inner
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: Theme.pad
                    spacing: 8

                    Label {
                        Layout.fillWidth: true
                        text: AppController.title + " — " + modelData.player
                        color: Theme.text
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                        maximumLineCount: 2
                        wrapMode: Text.WordWrap
                    }
                    BingoGrid {
                        Layout.fillWidth: true
                        availableWidth: parent.width - Theme.pad * 2
                        rows: modelData.cells
                        gageMode: AppController.gageMode
                    }
                }
            }
        }

        Item { Layout.preferredHeight: Theme.pad * 2 }
    }
}
