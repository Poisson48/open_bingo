import QtQuick
import QtQuick.Dialogs

Item {
    id: pickers

    function exportCurrent() {
        exportDialog.selectedFile = "file:///bingo-export.json"
        exportDialog.open()
    }
    function exportOne(projectId) {
        exportOneDialog.projectId = projectId
        exportOneDialog.selectedFile = "file:///bingo-projet.json"
        exportOneDialog.open()
    }
    function exportAll() {
        exportAllDialog.selectedFile = "file:///bingo-tous-projets.json"
        exportAllDialog.open()
    }
    function importOne() {
        importDialog.open()
    }
    function importAll() {
        importAllDialog.open()
    }

    FileDialog {
        id: exportDialog
        fileMode: FileDialog.SaveFile
        nameFilters: ["JSON (*.json)"]
        defaultSuffix: "json"
        onAccepted: AppController.exportCurrentJson(selectedFile)
    }
    FileDialog {
        id: exportOneDialog
        property string projectId: ""
        fileMode: FileDialog.SaveFile
        nameFilters: ["JSON (*.json)"]
        onAccepted: AppController.exportProjectJson(projectId, selectedFile)
    }
    FileDialog {
        id: exportAllDialog
        fileMode: FileDialog.SaveFile
        nameFilters: ["JSON (*.json)"]
        onAccepted: AppController.exportAllJson(selectedFile)
    }
    FileDialog {
        id: importDialog
        fileMode: FileDialog.OpenFile
        nameFilters: ["JSON (*.json)"]
        onAccepted: AppController.importJsonFile(selectedFile)
    }
    FileDialog {
        id: importAllDialog
        fileMode: FileDialog.OpenFile
        nameFilters: ["JSON (*.json)"]
        onAccepted: AppController.importAllJsonFile(selectedFile)
    }
}
