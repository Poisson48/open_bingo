import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    clip: true
    ColumnLayout {
        width: parent.width
        spacing: Theme.gap
        Label { text: "Aperçu impression A4 (2 grilles/page)"; color: Theme.textDim }
        Button {
            text: "Imprimer / PDF"
            enabled: AppController.grids.length > 0
            onClicked: AppController.printPreview()
        }
        Repeater {
            model: AppController.grids
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 200
                color: Theme.surface
                border.color: Theme.outline
                radius: 8
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    Label { text: AppController.title + " — " + modelData.player; color: Theme.text }
                    GridLayout {
                        columns: AppController.gridSize
                        Repeater {
                            model: modelData.cells
                            Repeater {
                                model: modelData
                                Rectangle {
                                    Layout.preferredWidth: 48
                                    Layout.preferredHeight: 36
                                    color: Theme.surfaceHigh
                                    Label { anchors.centerIn: parent; text: modelData.label; font.pixelSize: 8; color: Theme.text }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
