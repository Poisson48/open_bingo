import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page
    property string pageTitle: "Mes projets"
    property string deleteTargetId: ""
    property string deleteTargetTitle: ""

    property Component actions: Row {
        spacing: 6
        BingoButton {
            text: "Exporter"
            onClicked: AppController.pickExportAllJson()
        }
        BingoButton {
            text: "Importer"
            onClicked: AppController.pickImportAllJson()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pad
        spacing: Theme.gap

        ColoTextField {
            Layout.fillWidth: true
            hint: "Rechercher un projet…"
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
                    implicitHeight: cardCol.implicitHeight + 20
                    radius: Theme.radiusLg
                    color: Theme.surface
                    border.color: Theme.outline

                    ColumnLayout {
                        id: cardCol
                        anchors.fill: parent
                        anchors.margins: Theme.pad
                        spacing: 8

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Rectangle {
                                Layout.preferredWidth: 4
                                Layout.preferredHeight: 36
                                radius: 2
                                color: accent
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Label {
                                    Layout.fillWidth: true
                                    text: title
                                    color: Theme.text
                                    font.pixelSize: 16
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: description
                                    visible: description.length > 0
                                    color: Theme.textDim
                                    font.pixelSize: 13
                                    elide: Text.ElideRight
                                    maximumLineCount: 2
                                    wrapMode: Text.WordWrap
                                }
                                Label {
                                    text: gridSize + "×" + gridSize + " · "
                                          + playerCount + " j · " + caseCount + " cases"
                                    color: Theme.textDim
                                    font.pixelSize: 12
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Item { Layout.fillWidth: true }
                            BingoButton {
                                text: "Ouvrir"
                                primary: true
                                onClicked: AppController.openProject(projectId)
                            }
                            BingoButton {
                                text: "Suppr."
                                danger: true
                                onClicked: {
                                    page.deleteTargetId = projectId
                                    page.deleteTargetTitle = title
                                    confirmDelete.open()
                                }
                            }
                        }
                    }
                }
            }
        }

        BingoButton {
            Layout.fillWidth: true
            text: "+ Nouveau projet"
            primary: true
            onClicked: AppController.createProject()
        }
    }

    ColoDialog {
        id: confirmDelete
        title: "Supprimer ?"
        destructive: true
        acceptText: "Supprimer"
        Label {
            text: "Supprimer « " + page.deleteTargetTitle + " » ?"
            color: Theme.text
            wrapMode: Text.WordWrap
            width: parent.width
        }
        onAccepted: AppController.deleteProject(page.deleteTargetId)
    }
}
