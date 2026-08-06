import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page
    property string pageTitle: "Mes projets"
    property string deleteTargetId: ""
    property string deleteTargetTitle: ""

    property Component actions: Row {
        ToolButton {
            contentItem: Icon { name: "menu"; color: Theme.text }
            onClicked: importMenu.open()
        }
        Menu {
            id: importMenu
            MenuItem { text: "Exporter tout"; onTriggered: filePickers.exportAll() }
            MenuItem { text: "Importer"; onTriggered: filePickers.importAll() }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pad
        spacing: Theme.gap

        ColoTextField {
            Layout.fillWidth: true
            hint: "Rechercher…"
            onTextChanged: list.filter = text
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.gap
            property string filter: ""
            model: AppController.projects

            delegate: Item {
                width: list.width
                property bool matches: !list.filter
                    || title.toLowerCase().indexOf(list.filter.toLowerCase()) >= 0
                    || description.toLowerCase().indexOf(list.filter.toLowerCase()) >= 0
                height: matches ? card.implicitHeight : 0
                visible: matches

                Rectangle {
                    id: card
                    width: parent.width
                    implicitHeight: cardCol.implicitHeight + 24
                    radius: Theme.radius
                    color: Theme.surface
                    border.color: Theme.outline

                    RowLayout {
                        id: cardCol
                        anchors.fill: parent
                        anchors.margins: Theme.pad
                        Rectangle {
                            Layout.preferredWidth: 6
                            Layout.fillHeight: true
                            radius: 3
                            color: accent
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Label { text: title; color: Theme.text; font.pixelSize: 18; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight }
                            Label { text: description; color: Theme.textDim; visible: description.length > 0; Layout.fillWidth: true; elide: Text.ElideRight }
                            Label { text: gridSize + "×" + gridSize + " · " + playerCount + " joueurs · " + caseCount + " cases"; color: Theme.textDim; font.pixelSize: 12 }
                        }
                        Column {
                            spacing: 4
                            Button { text: "Ouvrir"; onClicked: AppController.openProject(projectId) }
                            Button {
                                flat: true; text: "Suppr."
                                onClicked: { page.deleteTargetId = projectId; page.deleteTargetTitle = title; confirmDelete.open() }
                            }
                        }
                    }
                }
            }
        }

        Button {
            Layout.fillWidth: true
            implicitHeight: Theme.touchTarget
            text: "+ Nouveau projet"
            onClicked: AppController.createProject()
        }
    }

    ColoDialog {
        id: confirmDelete
        title: "Supprimer ?"
        destructive: true
        acceptText: "Supprimer"
        Label { text: "Supprimer « " + page.deleteTargetTitle + " » ?"; color: Theme.text; wrapMode: Text.WordWrap }
        onAccepted: AppController.deleteProject(page.deleteTargetId)
    }

    FilePickers { id: filePickers; anchors.centerIn: parent }
}
