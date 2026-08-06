import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: sheet
    property string uri: ""

    function openForCurrent() {
        uri = AppController.buildShareUrl()
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
            text: "Partager « " + AppController.title + " »"
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
                source: uri.length > 0 ? "image://qr/" + uri : ""
            }
        }

        Label {
            Layout.fillWidth: true
            text: uri
            wrapMode: Text.WrapAnywhere
            color: Theme.textDim
            font.pixelSize: 11
        }

        ColoTextField {
            id: pasteUri
            Layout.fillWidth: true
            hint: "Coller un lien openbingo://join/…"
        }

        RowLayout {
            Layout.fillWidth: true
            BingoButton {
                Layout.fillWidth: true
                text: "Copier le lien"
                onClicked: AppController.copyToClipboard(uri)
            }
            BingoButton {
                Layout.fillWidth: true
                text: "Partager"
                primary: true
                onClicked: AppController.shareText(uri)
            }
        }

        BingoButton {
            Layout.fillWidth: true
            text: "Rejoindre (lien collé)"
            visible: pasteUri.text.trim().length > 0
            onClicked: {
                if (AppController.joinProjectUri(pasteUri.text.trim())) {
                    sheet.close()
                } else {
                    AppController.notify("Lien invalide")
                }
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
