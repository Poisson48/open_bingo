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
            onClicked: editorMenu.open()
        }
        Menu {
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
                labels: ["Config", "Phrases", "Grilles", "Impression", "Play"]
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
            GridsPage {}
            PrintPage {}
            PlayPage {}
        }
    }

    ShareSheet { id: shareSheet; anchors.centerIn: parent }

    Component.onCompleted: AppController.setKeepScreenOn(AppController.lastTab === 4)
    Connections {
        target: AppController
        function onLastTabChanged() {
            AppController.setKeepScreenOn(AppController.lastTab === 4)
        }
    }
}
