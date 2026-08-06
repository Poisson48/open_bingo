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
            text: "Export PDF A4 — 2 grilles par page (ouvrez le fichier pour imprimer)."
            color: Theme.textDim
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }
        BingoButton {
            text: "Exporter PDF"
            primary: true
            enabled: AppController.grids.length > 0
            onClicked: AppController.printPreview()
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
                    }
                }
            }
        }
    }
}
