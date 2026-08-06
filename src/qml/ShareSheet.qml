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
    width: parent.width
    y: parent.height - height
    modal: true
    padding: 20
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.6) }

    background: Rectangle {
        color: Theme.surface
        radius: 24
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
            font.pixelSize: 18
            font.weight: Font.DemiBold
        }

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: 220; height: 220
            radius: 12
            color: "white"
            Image {
                anchors.centerIn: parent
                width: 200; height: 200
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

        RowLayout {
            Layout.fillWidth: true
            Button {
                Layout.fillWidth: true
                text: "Copier le lien"
                onClicked: AppController.shareText(uri)
            }
            Button {
                flat: true
                text: "Fermer"
                onClicked: sheet.close()
            }
        }

        Label {
            Layout.fillWidth: true
            text: "Le lien contient la clé de chiffrement. Partagez-le uniquement avec les participants."
            wrapMode: Text.WordWrap
            color: Theme.warning
            font.pixelSize: 12
        }
    }
}
