import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page
    property string pageTitle: AppController.title

    property Component actions: Row {
        ToolButton {
            contentItem: Icon { name: "menu"; color: Theme.text }
            onClicked: editorMenu.open()
        }
        Menu {
            id: editorMenu
            MenuItem { text: "Partager"; onTriggered: shareSheet.openForCurrent() }
            MenuItem { text: "Exporter"; onTriggered: filePickers.exportCurrent() }
            MenuItem { text: "Importer"; onTriggered: filePickers.importOne() }
        }
    }

    function handleBack() { return false }

    TabBar {
        id: tabs
        width: parent.width
        currentIndex: AppController.lastTab
        onCurrentIndexChanged: AppController.lastTab = currentIndex
        TabButton { text: "Config" }
        TabButton { text: "Cases" }
        TabButton { text: "Grilles" }
        TabButton { text: "Impression" }
        TabButton { text: "Play" }
    }

    StackLayout {
        anchors.top: tabs.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        currentIndex: tabs.currentIndex

        ConfigPage {}
        CasesPage {}
        GridsPage {}
        PrintPage {}
        PlayPage {}
    }

    ShareSheet { id: shareSheet; anchors.centerIn: parent }
    FilePickers { id: filePickers; anchors.centerIn: parent }

    Component.onCompleted: AppController.setKeepScreenOn(tabs.currentIndex === 4)
    Connections {
        target: tabs
        function onCurrentIndexChanged() {
            AppController.setKeepScreenOn(tabs.currentIndex === 4)
        }
    }
}
