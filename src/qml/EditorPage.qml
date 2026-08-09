import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page
    property string pageTitle: AppController.title

    property Component actions: Row {
        spacing: 4
        IconButton {
            iconName: "menu"
            iconColor: Theme.text
            Accessible.name: "Menu projet"
            onClicked: editorMenu.popup()
        }
        ColoMenu {
            id: editorMenu
            MenuItem { text: "Partager"; onTriggered: shareSheet.openForCurrent() }
            MenuItem { text: "Exporter"; onTriggered: AppController.pickExportCurrentJson() }
            MenuItem { text: "Importer"; onTriggered: AppController.pickImportJson() }
        }
    }

    function handleBack() { return false }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 44
            color: Theme.surface
            BingoTabBar {
                id: tabBar
                anchors.fill: parent
                labels: ["Réglages", "Phrases", "Gages", "Grilles", "Impression", "Partie", "Classement"]
                currentIndex: AppController.lastTab
                onCurrentIndexChanged: {
                    if (AppController.lastTab !== currentIndex)
                        AppController.lastTab = currentIndex
                }
            }
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.outline
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: AppController.lastTab

            ConfigPage {}
            CasesPage {}
            GagesPage {}
            GridsPage {}
            PrintPage {}
            PlayPage { id: playTab }
            ScoresPage {}
        }
    }

    function openPlayFullscreen() {
        AppController.lastTab = 5
        playTab.openFullscreen()
    }

    function applyPlayChecks(playerName, c) {
        playTab.applyPlayChecks(playerName, c)
    }

    ShareSheet { id: shareSheet; anchors.centerIn: parent }

    Component.onCompleted: AppController.setKeepScreenOn(AppController.lastTab === 5)
    Connections {
        target: AppController
        function onLastTabChanged() {
            AppController.setKeepScreenOn(AppController.lastTab === 5)
        }
    }
}
