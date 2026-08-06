import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: window
    visible: true
    title: "Open Bingo"
    readonly property int forcedW: typeof bingoForcedW !== "undefined" ? bingoForcedW : 0
    readonly property int forcedH: typeof bingoForcedH !== "undefined" ? bingoForcedH : 0
    readonly property string screenshotDir: typeof bingoScreenshotDir !== "undefined"
                                            ? bingoScreenshotDir : ""
    readonly property bool screenshotMode: screenshotDir.length > 0
    width: forcedW > 0 ? forcedW : Screen.width
    height: forcedH > 0 ? forcedH : Screen.height
    minimumWidth: 320
    minimumHeight: 480
    color: Theme.background

    Component.onCompleted: {
        if (Qt.platform.os === "android")
            window.showMaximized()
        if (window.screenshotMode)
            screenshotRunner.start()
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

                    IconButton {
                        visible: !window.onProjects
                        iconName: "back"
                        iconColor: Theme.textDim
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
                              ? "Open Bingo"
                              : (stack.currentItem && stack.currentItem.pageTitle
                                 ? stack.currentItem.pageTitle : "Projet")
                        color: window.onProjects ? Theme.accent : Theme.text
                        font.pixelSize: window.onProjects ? 17 : 16
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

        Rectangle {
            width: parent.width
            height: visible ? 28 : 0
            visible: !window.screenshotMode && window.offline
            color: Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.92)
            Label {
                anchors.centerIn: parent
                text: "Hors ligne"
                color: "#1A1400"
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }
        }
        Rectangle {
            width: parent.width
            height: visible ? 24 : 0
            visible: !window.screenshotMode && !window.offline && window.pending
            color: Theme.surfaceHigh
            Label {
                anchors.centerIn: parent
                text: "Sync… " + AppController.pendingChanges
                color: Theme.textDim
                font.pixelSize: 11
            }
        }
        Rectangle {
            id: updateBanner
            width: parent.width
            height: visible ? 56 : 0
            visible: !window.screenshotMode
                     && (Updater.updateAvailable || Updater.downloading || Updater.readyToInstall)
            color: Qt.rgba(Theme.surfaceHigh.r, Theme.surfaceHigh.g, Theme.surfaceHigh.b, 0.95)
            clip: true

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.outline
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 6
                spacing: 8

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3

                    Label {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        color: Theme.text
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        text: {
                            if (Updater.downloading)
                                return "Téléchargement…"
                            if (Updater.readyToInstall)
                                return "Version " + Updater.latestVersion + " prête"
                            return "Version " + Updater.latestVersion + " disponible"
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 3
                        radius: 2
                        visible: Updater.downloading
                        color: Theme.outline

                        Rectangle {
                            height: parent.height
                            radius: 2
                            color: Theme.accent
                            width: parent.width * Updater.progress
                            Behavior on width { NumberAnimation { duration: 120 } }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: !Updater.downloading
                        elide: Text.ElideRight
                        color: Theme.textDim
                        font.pixelSize: 12
                        text: Updater.readyToInstall
                              ? "Android vous demandera confirmation"
                              : "Vous avez la " + Updater.currentVersion
                    }
                }

                Button {
                    flat: true
                    visible: !Updater.downloading
                    implicitHeight: Theme.touchTarget
                    contentItem: Label {
                        text: Updater.readyToInstall ? "Installer" : "Mettre à jour"
                        color: Theme.accent
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (Updater.readyToInstall)
                            Updater.install()
                        else if (Updater.releaseNotes.length > 0)
                            changelogDialog.openPending()
                        else
                            Updater.download()
                    }
                }

                IconButton {
                    visible: !Updater.downloading
                    iconName: "close"
                    iconColor: Theme.textDim
                    onClicked: Updater.dismiss()
                }
            }
        }
    }

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: projectsPage
    }

    Component { id: projectsPage; ProjectsPage {} }
    Component { id: editorPage; EditorPage {} }

    Connections {
        target: AppController
        function onEditorOpened(projectId) { stack.push(editorPage) }
        function onToast(message) { snackbar.show(message) }
    }

    // Après une mise à jour : montrer ce qui a changé depuis la dernière lecture.
    Connections {
        target: Updater
        function onChangelogChanged() {
            if (Updater.hasWhatsNew && !Updater.updateAvailable
                    && !whatsNewDialog.opened && !changelogDialog.opened)
                Qt.callLater(function () { whatsNewDialog.openWhatsNew() })
        }
    }

    ChangelogDialog { id: changelogDialog }
    ChangelogDialog { id: whatsNewDialog }

    function openChangelog() { changelogDialog.openHistory() }

    Timer {
        id: shotTimer
        repeat: false
        onTriggered: screenshotRunner.runStep()
    }

    // Mode capture pour README / page GitHub (BINGO_SCREENSHOT_DIR=/chemin).
    QtObject {
        id: screenshotRunner
        property int step: 0
        property string demoId: ""

        function start() {
            // Liste projets d'abord (avec la démo déjà créée), puis parcours des onglets.
            demoId = AppController.seedDemoProject()
            schedule(900)
        }

        function schedule(ms) {
            shotTimer.interval = ms
            shotTimer.restart()
        }

        function capture(name) {
            const path = window.screenshotDir + "/" + name
            if (AppController.saveScreenshot(path))
                console.log("Screenshot:", path)
            else
                console.warn("Screenshot failed:", path)
            step++
            if (step >= 10)
                Qt.quit()
            else
                schedule(400)
        }

        function runStep() {
            if (step === 0) {
                capture("01-projects.png")
            } else if (step === 1) {
                AppController.openProject(demoId)
                AppController.lastTab = 0
                step = 2
                schedule(700)
            } else if (step === 2) {
                capture("02-config.png")
            } else if (step === 3) {
                AppController.lastTab = 1
                step = 4
                schedule(450)
            } else if (step === 4) {
                capture("03-cases.png")
            } else if (step === 5) {
                AppController.lastTab = 2
                step = 6
                schedule(500)
            } else if (step === 6) {
                capture("04-grids.png")
            } else if (step === 7) {
                AppController.lastTab = 4
                step = 8
                schedule(550)
            } else if (step === 8) {
                capture("05-play.png")
            } else {
                Qt.quit()
            }
        }
    }

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
