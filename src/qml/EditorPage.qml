import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page
    property string pageTitle: AppController.title

    property Component actions: Row {
        spacing: 4
        ToolButton {
            contentItem: Icon { name: "menu"; color: Theme.text; size: 20 }
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
                anchors.fill: parent
                labels: ["Config", "Phrases", "Grilles", "Impression", "Play"]
                currentIndex: tabs.currentIndex
                onCurrentIndexChanged: tabs.currentIndex = currentIndex
            }
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.outline
            }
        }

        Item {
            id: tabs
            Layout.fillWidth: true
            Layout.fillHeight: true
            property int currentIndex: AppController.lastTab
            onCurrentIndexChanged: AppController.lastTab = currentIndex

            StackLayout {
                anchors.fill: parent
                currentIndex: tabs.currentIndex
                ConfigPage {}
                CasesPage {}
                GridsPage {}
                PrintPage {}
                PlayPage {}
            }
        }
    }

    ShareSheet { id: shareSheet; anchors.centerIn: parent }

    Component.onCompleted: AppController.setKeepScreenOn(tabs.currentIndex === 4)
    Connections {
        target: tabs
        function onCurrentIndexChanged() {
            AppController.setKeepScreenOn(tabs.currentIndex === 4)
        }
    }
}
