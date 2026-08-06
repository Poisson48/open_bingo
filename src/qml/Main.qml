import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: window
    visible: true
    title: "Open Bingo"
    width: 420
    height: 800
    color: Theme.background

    Material.theme: Theme.dark ? Material.Dark : Material.Light
    Material.background: Theme.background
    Material.foreground: Theme.text
    Material.accent: Theme.accent

    readonly property bool offline: !AppController.online
    readonly property bool pending: AppController.pendingChanges > 0

    onClosing: function (close) {
        close.accepted = false
        const page = stack.currentItem
        if (page && typeof page.handleBack === "function" && page.handleBack())
            return
        if (stack.depth > 1) {
            stack.pop()
            return
        }
        close.accepted = true
    }

    header: Rectangle {
        color: Theme.surface
        implicitHeight: 56
        RowLayout {
            anchors.fill: parent
            anchors.margins: 4
            ToolButton {
                visible: stack.depth > 1
                contentItem: Icon { name: "back"; color: Theme.text; size: 22 }
                onClicked: stack.pop()
            }
            Label {
                Layout.fillWidth: true
                text: stack.currentItem && stack.currentItem.pageTitle
                      ? stack.currentItem.pageTitle : "Mes projets"
                color: Theme.text
                font.pixelSize: 20
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Loader {
                sourceComponent: stack.currentItem && stack.currentItem.actions
                                 ? stack.currentItem.actions : null
            }
        }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.outline }
    }

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: projectsPage
    }

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        z: 10
        Rectangle {
            width: parent.width
            height: window.offline ? 32 : 0
            visible: height > 0
            color: Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.92)
            Label {
                anchors.centerIn: parent
                text: "Hors ligne — sync au retour du réseau"
                color: "#1A1400"
                font.pixelSize: 12
            }
        }
        Rectangle {
            width: parent.width
            height: visible ? 26 : 0
            visible: !window.offline && window.pending
            color: Theme.surfaceHigh
            Label {
                anchors.centerIn: parent
                text: "Envoi de " + AppController.pendingChanges + " modification(s)…"
                color: Theme.textDim
                font.pixelSize: 12
            }
        }
        Rectangle {
            width: parent.width
            height: visible ? 48 : 0
            visible: Updater.updateAvailable || Updater.readyToInstall
            color: Theme.surfaceHigh
            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                Label {
                    Layout.fillWidth: true
                    text: Updater.readyToInstall
                          ? "Version " + Updater.latestVersion + " prête"
                          : "Mise à jour " + Updater.latestVersion
                    color: Theme.text
                }
                Button {
                    flat: true
                    text: Updater.readyToInstall ? "Installer" : "Mettre à jour"
                    onClicked: Updater.readyToInstall ? Updater.install() : Updater.download()
                }
            }
        }
    }

    Component { id: projectsPage; ProjectsPage {} }
    Component { id: editorPage; EditorPage {} }

    Connections {
        target: AppController
        function onEditorOpened(projectId) {
            stack.push(editorPage)
        }
        function onToast(message) { snackbar.show(message) }
    }

    ChangelogDialog { id: changelogDialog }

    Popup {
        id: snackbar
        y: parent.height - height - 24
        x: 16
        width: parent.width - 32
        padding: 14
        modal: false
        property string message: ""
        function show(text) { message = text; open(); hideTimer.restart() }
        Timer { id: hideTimer; interval: 2800; onTriggered: snackbar.close() }
        background: Rectangle { color: Theme.surfaceHigh; radius: 12; border.color: Theme.outline }
        contentItem: Label { text: snackbar.message; color: Theme.text; wrapMode: Text.WordWrap }
    }
}
