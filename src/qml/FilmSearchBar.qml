import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root
    spacing: Theme.gap

    property alias text: queryField.text
    property bool busy: false
    property bool searchEnabled: true

    signal searchRequested

    ColoTextField {
        id: queryField
        Layout.fillWidth: true
        hint: "Titre du film…"
        enabled: root.searchEnabled && !root.busy
        Keys.onReturnPressed: root.searchRequested()
        Keys.onEnterPressed: root.searchRequested()
    }

    BingoButton {
        text: root.busy ? "…" : "Chercher"
        primary: true
        enabled: root.searchEnabled && !root.busy && queryField.text.trim().length > 0
        Layout.preferredWidth: 110
        onClicked: root.searchRequested()
    }
}
