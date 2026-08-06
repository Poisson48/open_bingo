import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Overlay plein écran pour le mode « Sans papier » (comme l'ancienne app web).
Popup {
    id: fs
    property string playerName: ""
    property var rows: []
    property var checks: []

    signal checksUpdated(var newChecks)

    parent: Overlay.overlay
    modal: true
    padding: 0
    closePolicy: Popup.NoAutoClose
    Overlay.modal: Rectangle { color: Theme.background }

    onOpened: {
        width = Overlay.overlay.width
        height = Overlay.overlay.height
        x = 0
        y = 0
        AppController.setKeepScreenOn(true)
        AppController.lockLandscape()
        AppController.setImmersive(true)
    }
    onClosed: {
        AppController.setImmersive(false)
        AppController.unlockOrientation()
        AppController.setKeepScreenOn(false)
    }

    function bingoKey(r, c) { return r + "," + c }

    readonly property var bingoSet: {
        var set = {}
        const lines = AppController.detectBingoLines(fs.checks)
        for (var i = 0; i < lines.length; ++i) {
            const line = lines[i]
            for (var j = 0; j < line.length; ++j) {
                const p = line[j]
                set[bingoKey(p[0], p[1])] = true
            }
        }
        return set
    }

    readonly property int checkedCount: {
        var n = 0
        for (var r = 0; r < checks.length; ++r)
            for (var c = 0; c < (checks[r] || []).length; ++c)
                if (checks[r][c]) ++n
        return n
    }

    contentItem: ColumnLayout {
        width: fs.width
        height: fs.height
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 48
            color: Theme.surface
            border.color: Theme.outline
            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                Label {
                    Layout.fillWidth: true
                    text: fs.playerName
                    color: Theme.accent
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
                Label {
                    text: fs.checkedCount + " cochées"
                    color: Theme.textDim
                    font.pixelSize: 12
                }
                BingoButton {
                    text: "✕ Quitter"
                    onClicked: fs.close()
                }
            }
        }

        Item {
            id: gridHost
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 6

            BingoGrid {
                anchors.centerIn: parent
                availableWidth: gridHost.width
                availableHeight: gridHost.height
                rows: fs.rows
                interactive: true
                checks: fs.checks
                bingoSet: fs.bingoSet
                gageMode: AppController.gageMode
                gages: AppController.gages
                onCellClicked: function(r, c) {
                    var copy = fs.checks.slice(0)
                    if (!copy[r]) return
                    copy[r] = copy[r].slice(0)
                    copy[r][c] = !copy[r][c]
                    fs.checks = copy
                    fs.checksUpdated(copy)
                    AppController.vibrate()
                }
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.margins: 8
            visible: gridHost.height < gridHost.width * 0.55
            text: "Tournez le téléphone en paysage pour une grille plus grande"
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            color: Theme.warning
            font.pixelSize: 12
        }
    }
}
