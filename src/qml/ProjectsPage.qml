import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Item {
    id: page
    property string pageTitle: "Mes projets"
    property string deleteTargetId: ""
    property string deleteTargetTitle: ""
    property string leaveTargetId: ""
    property string leaveTargetTitle: ""
    property string menuProjectId: ""
    property string menuProjectTitle: ""
    property bool menuShared: false

    // Comme Colo Courses : Rejoindre bien visible dans la barre.
    property Component actions: Row {
        spacing: 4
        BingoButton {
            text: "Rejoindre"
            primary: true
            onClicked: joinDialog.open()
        }
        IconButton {
            iconName: "menu"
            iconColor: Theme.text
            Accessible.name: "Menu"
            onClicked: overflowMenu.popup()
        }
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
            text: "Relais de synchronisation"
            onTriggered: relaysDialog.open()
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

    ColoMenu {
        id: cardMenu
        MenuItem {
            text: "Dupliquer"
            onTriggered: {
                const id = AppController.cloneProject(page.menuProjectId)
                if (!id || id.length === 0)
                    AppController.notify("Impossible de dupliquer ce projet")
            }
        }
        MenuItem {
            text: "Partager (QR / lien)"
            onTriggered: shareSheet.openFor(page.menuProjectId, page.menuProjectTitle)
        }
        MenuItem {
            text: "Quitter la partie"
            enabled: page.menuShared
            onTriggered: {
                page.leaveTargetId = page.menuProjectId
                page.leaveTargetTitle = page.menuProjectTitle
                leaveDialog.open()
            }
        }
        MenuItem {
            text: "Supprimer"
            onTriggered: {
                page.deleteTargetId = page.menuProjectId
                page.deleteTargetTitle = page.menuProjectTitle
                confirmDelete.open()
            }
        }
    }

    ShareSheet { id: shareSheet }

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
            text: "Aucun projet. Rejoignez une partie avec un QR / lien, ou créez-en une."
            color: Theme.textDim
            wrapMode: Text.WordWrap
            font.pixelSize: 14
        }

        RowLayout {
            Layout.fillWidth: true
            visible: AppController.projects.count === 0
            spacing: Theme.gap
            BingoButton {
                Layout.fillWidth: true
                text: "Rejoindre"
                primary: true
                onClicked: joinDialog.open()
            }
            BingoButton {
                Layout.fillWidth: true
                text: "Démo cinéma"
                onClicked: {
                    const id = AppController.seedDemoProject()
                    if (id.length)
                        AppController.openProject(id)
                }
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
                    border.color: shared ? Theme.accent : Theme.outline
                    border.width: shared ? 1.5 : 1

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
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label {
                                        Layout.fillWidth: true
                                        text: title
                                        color: Theme.text
                                        font.pixelSize: 16
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                        maximumLineCount: 1
                                    }
                                    Rectangle {
                                        visible: shared
                                        radius: 6
                                        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.2)
                                        implicitWidth: sharedLbl.implicitWidth + 12
                                        implicitHeight: 20
                                        Label {
                                            id: sharedLbl
                                            anchors.centerIn: parent
                                            text: "Partagé"
                                            color: Theme.accent
                                            font.pixelSize: 11
                                            font.weight: Font.DemiBold
                                        }
                                    }
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
                                    text: (gridRows || gridSize) + "×" + (gridCols || gridSize) + "  "
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
                            IconButton {
                                iconName: "menu"
                                iconColor: Theme.textDim
                                Accessible.name: "Options du projet"
                                onClicked: {
                                    page.menuProjectId = projectId
                                    page.menuProjectTitle = title
                                    page.menuShared = !!shared
                                    cardMenu.popup()
                                }
                            }
                            Item { Layout.fillWidth: true }
                            BingoButton {
                                text: "Partager"
                                onClicked: shareSheet.openFor(projectId, title)
                            }
                            BingoButton {
                                text: "Ouvrir"
                                primary: true
                                onClicked: AppController.openProject(projectId)
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

        Item { Layout.preferredHeight: Theme.pad * 2 }
    }

    ColoDialog {
        id: joinDialog
        title: "Rejoindre une partie"
        acceptText: "Rejoindre"
        acceptEnabled: uriField.text.trim().length > 0

        BingoButton {
            Layout.fillWidth: true
            text: "Scanner le QR code"
            primary: true
            onClicked: {
                joinDialog.close()
                scanPopup.open()
            }
        }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: "ou collez le lien reçu"
            color: Theme.textDim
            font.pixelSize: 13
        }

        ColoTextField {
            id: uriField
            Layout.fillWidth: true
            hint: "openbingo://join/1/…"
            onAccepted: if (joinDialog.acceptEnabled) joinDialog.accept()
        }

        onOpened: {
            uriField.text = ""
            uriField.forceActiveFocus()
        }
        onAccepted: {
            if (!AppController.joinProjectUri(uriField.text.trim()))
                AppController.notify("Lien d'invitation invalide")
        }
    }

    Popup {
        id: scanPopup
        parent: Overlay.overlay
        width: parent.width
        height: parent.height
        padding: 0
        modal: true
        closePolicy: Popup.CloseOnEscape
        background: Rectangle { color: "black" }

        Loader {
            id: scanLoader
            anchors.fill: parent
            source: scanPopup.opened ? "ScanPage.qml" : ""

            onStatusChanged: {
                if (status === Loader.Error) {
                    scanPopup.close()
                    AppController.notify("Caméra indisponible — collez le lien à la place")
                    joinDialog.open()
                }
            }
        }

        Connections {
            target: scanLoader.item
            ignoreUnknownSignals: true
            function onJoined() { scanPopup.close() }
            function onCloseRequested() { scanPopup.close() }
        }
    }

    ColoDialog {
        id: leaveDialog
        title: "Quitter la partie ?"
        acceptText: "Quitter"
        destructive: true

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.textDim
            font.pixelSize: 14
            text: "« " + page.leaveTargetTitle + " » sera effacé de cet appareil. "
                  + "Les autres participants le gardent, et tu pourras le rejoindre à nouveau avec le lien ou le QR."
        }
        onAccepted: AppController.leaveProject(page.leaveTargetId)
    }

    ColoDialog {
        id: confirmDelete
        title: "Supprimer ?"
        destructive: true
        acceptText: "Supprimer"
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: "Supprimer « " + page.deleteTargetTitle + " » ?"
            color: Theme.text
        }
        onAccepted: AppController.deleteProject(page.deleteTargetId)
    }

    ColoDialog {
        id: relaysDialog
        title: "Relais de synchronisation"
        acceptText: "Enregistrer"

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.textDim
            font.pixelSize: 13
            text: "Adresses WebSocket des relais Nostr (wss://…). "
                  + "Une URL par ligne."
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(120,
                                            Math.min(relaysField.implicitHeight + 16,
                                                     relaysDialog.scrollMaxHeight))
            clip: true

            TextArea {
                id: relaysField
                width: parent.width
                wrapMode: TextArea.Wrap
                color: Theme.text
                font.pixelSize: 14
                font.family: "monospace"
                selectByMouse: true
                placeholderText: AppController.defaultRelayUrls()
                background: Rectangle {
                    radius: 12
                    color: Theme.surfaceHigh
                    border.color: Theme.outline
                }
            }
        }

        Button {
            Layout.fillWidth: true
            flat: true
            implicitHeight: 40
            contentItem: Label {
                text: "Rétablir par défaut"
                color: Theme.accent
                horizontalAlignment: Text.AlignHCenter
            }
            onClicked: relaysField.text = AppController.defaultRelayUrls()
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.textDim
            font.pixelSize: 13
            text: "Notifications push (Android, app fermée)"
        }

        Switch {
            id: pushEnabledSwitch
            text: "Activer la veille push"
        }

        ColoTextField {
            id: pushUrlField
            Layout.fillWidth: true
            placeholderText: AppController.defaultPushBaseUrl()
            enabled: pushEnabledSwitch.checked
        }

        onOpened: {
            relaysField.text = AppController.relayUrls
            pushEnabledSwitch.checked = AppController.pushEnabled
            pushUrlField.text = AppController.pushBaseUrl
        }
        onAccepted: {
            AppController.setRelayUrls(relaysField.text)
            AppController.setPushSettings(pushEnabledSwitch.checked, pushUrlField.text)
        }
    }
}
