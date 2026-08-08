import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Feuille de partage Colo-style : QR face-à-face + lien à envoyer.
// Le lien contient la clé — d'où l'avertissement. Pas de « rejoindre » ici.
Popup {
    id: sheet
    property string projectId: ""
    property string projectTitle: ""
    property string uri: ""

    function openForCurrent() {
        openFor(AppController.currentProjectId, AppController.title)
    }

    function openFor(id, name) {
        projectId = id || ""
        projectTitle = name || ""
        uri = projectId.length > 0
              ? AppController.joinUriForProject(projectId)
              : AppController.buildShareUrl()
        open()
    }

    parent: Overlay.overlay
    width: Math.min(Overlay.overlay.width - 32, 480)
    x: (Overlay.overlay.width - width) / 2
    y: Math.max(16, Overlay.overlay.height - height - 16)
    modal: true
    padding: 20
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.6) }

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusLg
        border.color: Theme.outline
    }

    ColumnLayout {
        width: parent.width - 40
        spacing: Theme.gap

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: "Partager « " + sheet.projectTitle + " »"
            color: Theme.text
            font.pixelSize: 17
            font.weight: Font.DemiBold
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: Math.min(220, parent.width - 40)
            height: width
            radius: Theme.radiusLg
            color: "white"
            Image {
                anchors.centerIn: parent
                width: parent.width - 20
                height: parent.height - 20
                // encodeURIComponent : sinon openbingo://… casse le parsing image://
                source: uri.length > 0 ? "image://qr/" + encodeURIComponent(uri) : ""
                fillMode: Image.PreserveAspectFit
                smooth: false
            }
        }

        Label {
            Layout.fillWidth: true
            visible: uri.length > 0
            text: uri
            wrapMode: Text.WrapAnywhere
            color: Theme.textDim
            font.pixelSize: 11
        }

        Label {
            Layout.fillWidth: true
            visible: uri.length === 0
            text: "Impossible de générer le lien — ouvrez un projet d'abord."
            color: Theme.danger
            wrapMode: Text.WordWrap
            font.pixelSize: 13
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap
            BingoButton {
                Layout.fillWidth: true
                text: "Copier le lien"
                enabled: uri.length > 0
                onClicked: {
                    AppController.copyToClipboard(uri)
                    AppController.notify("Lien copié")
                }
            }
            BingoButton {
                Layout.fillWidth: true
                text: "Envoyer"
                primary: true
                enabled: uri.length > 0
                onClicked: AppController.shareText(uri)
            }
        }

        Label {
            Layout.fillWidth: true
            text: "Le lien contient la clé de chiffrement — partagez-le uniquement avec les participants."
            wrapMode: Text.WordWrap
            color: Theme.warning
            font.pixelSize: 12
        }
    }
}
