import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Notes de version — comme Colo Tâches : modal avant téléchargement + « quoi de neuf » après maj.
ColoDialog {
    id: dlg

    property string mode: "history" // pending | whatsNew | history

    title: mode === "pending" ? ("Nouveautés — " + Updater.latestVersion)
         : mode === "whatsNew" ? ("Quoi de neuf — " + Updater.currentVersion)
         : "Notes de version"
    acceptText: mode === "pending" ? "Télécharger"
              : mode === "whatsNew" ? "Compris"
              : "Fermer"
    showCancel: mode === "pending"
    destructive: false
    width: Math.min(Overlay.overlay ? Overlay.overlay.width - 40 : 400, 440)

    readonly property string bodyText: {
        if (mode === "pending")
            return Updater.releaseNotes
        if (mode === "whatsNew")
            return Updater.whatsNewNotes
        let blocks = []
        for (let i = 0; i < Updater.changelog.length; ++i) {
            const e = Updater.changelog[i]
            const ver = e.version || ""
            const notes = (e.notes || "").trim()
            if (!ver)
                continue
            blocks.push(notes.length > 0
                        ? ("Version " + ver + "\n\n" + notes)
                        : ("Version " + ver))
        }
        return blocks.join("\n\n————————————\n\n")
    }

    function openPending()  { mode = "pending";  open() }
    function openWhatsNew() { mode = "whatsNew"; open() }
    function openHistory()  { mode = "history";  open() }

    Label {
        Layout.fillWidth: true
        visible: mode === "pending"
        text: "Vous avez la " + Updater.currentVersion + ". Notes de la nouvelle version :"
        color: Theme.textDim
        font.pixelSize: 13
        wrapMode: Text.WordWrap
    }

    Label {
        Layout.fillWidth: true
        visible: dlg.bodyText.length === 0
        text: mode === "history"
              ? "Aucune note de version pour l'instant."
              : "Corrections et améliorations."
        color: Theme.textDim
        font.pixelSize: 14
        wrapMode: Text.WordWrap
    }

    Rectangle {
        Layout.fillWidth: true
        visible: dlg.bodyText.length > 0
        implicitHeight: Math.min(Math.max(notesCol.implicitHeight + 24, 100), 360)
        radius: Theme.radiusLg
        color: Theme.inputBg
        border.color: Theme.outline
        clip: true

        Flickable {
            anchors.fill: parent
            anchors.margins: 12
            contentHeight: notesCol.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollIndicator.vertical: ScrollIndicator {}

            Column {
                id: notesCol
                width: parent.width
                spacing: 8
                Label {
                    width: parent.width
                    text: dlg.bodyText
                    color: Theme.text
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                    lineHeight: 1.35
                }
            }
        }
    }

    onAccepted: {
        if (mode === "pending")
            Updater.download()
        else if (mode === "whatsNew")
            Updater.acknowledgeNotes()
    }
}
