import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    clip: true
    ColumnLayout {
        width: parent.width
        anchors.margins: Theme.pad
        spacing: Theme.gap

        ColoTextField {
            Layout.fillWidth: true
            text: AppController.title
            hint: "Titre"
            onEditingFinished: AppController.title = text
        }
        ColoTextField {
            Layout.fillWidth: true
            text: AppController.description
            hint: "Description"
            onEditingFinished: AppController.description = text
        }

        RowLayout {
            Label { text: "Taille grille"; color: Theme.text; Layout.fillWidth: true }
            SpinBox {
                from: 2; to: 12; value: AppController.gridSize
                onValueModified: AppController.gridSize = value
            }
        }

        CheckBox {
            text: "Case FREE au centre"
            checked: AppController.freeCenter
            onCheckedChanged: AppController.freeCenter = checked
        }
        CheckBox {
            text: "Mode gage"
            checked: AppController.gageMode
            onCheckedChanged: AppController.gageMode = checked
        }

        RowLayout {
            visible: !AppController.gageMode
            Label { text: "HP départ"; color: Theme.text }
            SpinBox { from: 1; to: 100; value: AppController.startHP
                onValueModified: AppController.startHP = value }
        }

        Label { text: "Joueurs"; color: Theme.text; font.weight: Font.DemiBold }
        Repeater {
            model: AppController.players
            RowLayout {
                Layout.fillWidth: true
                ColoTextField {
                    Layout.fillWidth: true
                    text: modelData.name
                    onEditingFinished: AppController.setPlayerName(index, text)
                }
                ToolButton {
                    text: "✕"
                    onClicked: AppController.removePlayer(index)
                }
            }
        }
        Button { text: "+ Joueur"; onClicked: AppController.addPlayer() }

        Button {
            Layout.fillWidth: true
            text: "Enregistrer la configuration"
            onClicked: AppController.saveConfig()
        }
    }
}
