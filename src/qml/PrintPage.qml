import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    clip: true
    contentWidth: availableWidth

    ColumnLayout {
        width: availableWidth - Theme.pad * 2
        spacing: Theme.gap

        Label {
            Layout.fillWidth: true
            text: "Aperçu impression A4 (2 grilles/page)"
            color: Theme.textDim
            font.pixelSize: 13
        }
        BingoButton {
            text: "Imprimer / PDF"
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
                    }
                    BingoGrid {
                        Layout.fillWidth: true
                        availableWidth: parent.width - Theme.pad * 2
                        rows: modelData.cells
                    }                }
            }
        }
    }
}
