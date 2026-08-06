import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Item {
    id: page
    property string pageTitle: "Mes projets"
    property string deleteTargetId: ""
    property string deleteTargetTitle: ""

    // Actions dans le menu : laisse la place au titre « Open Bingo ».
    property Component actions: IconButton {
        iconName: "menu"
        iconColor: Theme.text
        onClicked: overflowMenu.popup()
    }

    ColoMenu {
        id: overflowMenu
        MenuItem {
            text: "Exporter tout"
            onTriggered: AppController.pickExportAllJson()
        }
        MenuItem {
            text: "Importer"
            onTriggered: AppController.pickImportAllJson()
        }
        MenuItem {
            text: "Notes de version"
            onTriggered: {
                const w = page.Window.window
                if (w && typeof w.openChangelog === "function")
                    w.openChangelog()
            }
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

        Label {
            Layout.fillWidth: true
            visible: AppController.projects.count === 0
            text: "Aucun projet. Chargez la démo pour voir un bingo jouable tout de suite."
            color: Theme.textDim
            wrapMode: Text.WordWrap
            font.pixelSize: 14
        }

        BingoButton {
            Layout.fillWidth: true
            visible: AppController.projects.count === 0
            text: "Charger le projet démo"
            primary: true
            onClicked: {
                const id = AppController.seedDemoProject()
                if (id.length)
                    AppController.openProject(id)
            }
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
                                    Layout.fillWidth: true
                                    text: gridSize + "x" + gridSize + "  "
                                          + playerCount + " joueurs  " + caseCount + " phrases"
                                          + (gridCount > 0 ? ("  " + gridCount + " grilles") : "  — pas de grille")
                                    color: gridCount > 0 ? Theme.textDim : Theme.warning
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
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

        BingoButton {
            Layout.fillWidth: true
            text: "Charger un projet démo cinéma"
            onClicked: {
                const id = AppController.seedDemoProject()
                if (id.length)
                    AppController.openProject(id)
            }
        }

        // Marge au-dessus de la barre de geste Android.
        Item { Layout.preferredHeight: Theme.pad * 2 }
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
