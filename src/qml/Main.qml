import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: window
    visible: true
    title: "Open Bingo"
    readonly property int forcedW: {
        const v = Qt.environmentVariable("BINGO_TEST_W")
        return v.length ? parseInt(v) : 0
    }
    readonly property int forcedH: {
        const v = Qt.environmentVariable("BINGO_TEST_H")
        return v.length ? parseInt(v) : 0
    }
    width: forcedW > 0 ? forcedW : Screen.width
    height: forcedH > 0 ? forcedH : Screen.height
    minimumWidth: 320
    minimumHeight: 480
    color: Theme.background

    Component.onCompleted: {
        if (Qt.platform.os === "android")
            window.showMaximized()
    }

    Material.theme: Material.Dark
    Material.background: Theme.background
    Material.foreground: Theme.text
    Material.accent: Theme.accent

    readonly property bool offline: !AppController.online
    readonly property bool pending: AppController.pendingChanges > 0
    readonly property bool onProjects: stack.depth <= 1

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

    header: Column {
        width: parent.width
        spacing: 0

        Rectangle {
            width: parent.width
            height: headerRow.implicitHeight + 16
            color: Theme.surface
            border.color: Theme.outline
            border.width: 0
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.outline
            }

            ColumnLayout {
                id: headerRow
                anchors.fill: parent
                anchors.margins: 8
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    ToolButton {
                        visible: !window.onProjects
                        implicitWidth: 40
                        contentItem: Label {
                            text: "← Projets"
                            color: Theme.textDim
                            font.pixelSize: 12
                        }
                        onClicked: stack.pop()
                    }

                    Image {
                        visible: window.onProjects
                        source: "qrc:/icons/openbingo.png"
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                    }

                    Label {
                        Layout.fillWidth: true
                        text: window.onProjects
                              ? "Générateur de Bingo"
                              : (stack.currentItem && stack.currentItem.pageTitle
                                 ? stack.currentItem.pageTitle : "Projet")
                        color: window.onProjects ? Theme.accent : Theme.text
                        font.pixelSize: window.onProjects ? 15 : 16
                        font.weight: Font.Bold
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }

                    Loader {
                        Layout.alignment: Qt.AlignVCenter
                        sourceComponent: stack.currentItem && stack.currentItem.actions
                                         ? stack.currentItem.actions : null
                    }
                }
            }
        }
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
        topPadding: header.height

        Rectangle {
            width: parent.width
            height: window.offline ? 28 : 0
            visible: height > 0
            color: Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.92)
            Label {
                anchors.centerIn: parent
                text: "Hors ligne"
                color: "#1A1400"
                font.pixelSize: 12
            }
        }
        Rectangle {
            width: parent.width
            height: visible ? 24 : 0
            visible: !window.offline && window.pending
            color: Theme.surfaceHigh
            Label {
                anchors.centerIn: parent
                text: "Sync… " + AppController.pendingChanges
                color: Theme.textDim
                font.pixelSize: 11
            }
        }
        Rectangle {
            width: parent.width
            height: visible ? 44 : 0
            visible: Updater.updateAvailable || Updater.readyToInstall
            color: Theme.surfaceHigh
            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                Label {
                    Layout.fillWidth: true
                    text: Updater.readyToInstall
                          ? "v" + Updater.latestVersion + " prête"
                          : "Mise à jour v" + Updater.latestVersion
                    color: Theme.text
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }
                BingoButton {
                    text: Updater.readyToInstall ? "Installer" : "Télécharger"
                    primary: true
                    onClicked: Updater.readyToInstall ? Updater.install() : Updater.download()
                }
            }
        }
    }

    Component { id: projectsPage; ProjectsPage {} }
    Component { id: editorPage; EditorPage {} }

    Connections {
        target: AppController
        function onEditorOpened(projectId) { stack.push(editorPage) }
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
        background: Rectangle {
            color: Theme.surface
            radius: Theme.radiusLg
            border.color: Theme.outline
        }
        contentItem: Label {
            text: snackbar.message
            color: Theme.text
            wrapMode: Text.WordWrap
            font.pixelSize: 14
        }
    }
}
