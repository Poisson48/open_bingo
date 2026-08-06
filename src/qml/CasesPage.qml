import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    clip: true
    ColumnLayout {
        width: parent.width
        spacing: Theme.gap
        anchors.margins: Theme.pad

        Label {
            text: AppController.cases.length + " cases · " + AppController.availableCells + " cellules"
            color: Theme.textDim
        }
        Label {
            visible: AppController.gridsDirty
            text: "⚠ Les grilles ne reflètent plus les cases — regénérez-les."
            color: Theme.warning
            wrapMode: Text.WordWrap
        }

        RowLayout {
            ColoTextField { id: newLabel; Layout.fillWidth: true; hint: "Nouvelle phrase" }
            SpinBox { id: pts; from: 0; to: 99; value: 1 }
            SpinBox { id: rate; from: 0; to: 100; value: 50; suffix: " %" }
            Button {
                text: "+"
                onClicked: {
                    if (newLabel.text.trim().length === 0) return
                    AppController.addCase(newLabel.text, pts.value, rate.value)
                    newLabel.text = ""
                }
            }
        }

        Repeater {
            model: AppController.cases
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 56
                radius: 12
                color: Theme.surface
                border.color: Theme.outline
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    Label { Layout.fillWidth: true; text: modelData.label; color: Theme.text; elide: Text.ElideRight }
                    Label { text: modelData.points + " pt · " + modelData.rate + "%"; color: Theme.textDim }
                    ToolButton { text: "✕"; onClicked: AppController.removeCase(index) }
                }
            }
        }

        Label { text: "Gages"; color: Theme.text; font.weight: Font.DemiBold; visible: AppController.gageMode || AppController.gages.length > 0 }
        RowLayout {
            ColoTextField { id: gageDesc; Layout.fillWidth: true; hint: "Description gage" }
            SpinBox { id: gageHp; from: 0; to: 100; value: 5 }
            Button { text: "+"; onClicked: AppController.addGage(gageDesc.text, gageHp.value) }
        }
        Repeater {
            model: AppController.gages
            Label { text: (index + 1) + ". " + modelData.description; color: Theme.text; wrapMode: Text.WordWrap }
        }
    }
}
