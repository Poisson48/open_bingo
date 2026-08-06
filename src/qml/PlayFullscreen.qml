import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

// Overlay plein écran « Sans papier » — taille liée à l'overlay (rotation paysage OK).
Popup {
    id: fs
    property string playerName: ""
    property var rows: []
    property var checks: []
    property int playerIndex: -1

    signal checksUpdated(var newChecks)

    parent: Overlay.overlay
    modal: true
    dim: true
    focus: true
    padding: 0
    closePolicy: Popup.NoAutoClose
    Overlay.modal: Rectangle { color: Theme.background }

    readonly property real overlayW: Overlay.overlay ? Overlay.overlay.width : Screen.width
    readonly property real overlayH: Overlay.overlay ? Overlay.overlay.height : Screen.height
    x: 0
    y: 0
    width: overlayW
    height: overlayH

    onOpened: {
        AppController.setKeepScreenOn(true)
        AppController.lockLandscape()
        AppController.setImmersive(true)
    }
    onClosed: {
        AppController.setImmersive(false)
        AppController.unlockOrientation()
        // L'onglet Play remet keep-screen-on si on y reste.
        AppController.setKeepScreenOn(AppController.lastTab === 4)
    }

    function bingoKey(r, c) { return r + "," + c }

    function deepCopyChecks(src) {
        var out = []
        for (var r = 0; r < src.length; ++r) {
            var row = src[r] || []
            var copy = []
            for (var c = 0; c < row.length; ++c)
                copy.push(!!row[c])
            out.push(copy)
        }
        return out
    }

    function toggle(r, c) {
        if (!checks || !checks[r])
            return
        const mid = Math.floor(AppController.gridSize / 2)
        if (AppController.freeCenter && AppController.gridSize % 2 === 1
                && r === mid && c === mid)
            return
        var copy = deepCopyChecks(checks)
        copy[r][c] = !copy[r][c]
        if (AppController.freeCenter && AppController.gridSize % 2 === 1
                && mid < copy.length && mid < (copy[mid] || []).length)
            copy[mid][mid] = true
        checks = copy
        bingoRevision++
        checksUpdated(copy)
        AppController.vibrate()
    }

    property int bingoRevision: 0

    readonly property var bingoSet: {
        void bingoRevision
        void checks
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
        for (var r = 0; r < checks.length; ++r) {
            const row = checks[r] || []
            for (var c = 0; c < row.length; ++c)
                if (row[c]) ++n
        }
        return n
    }

    readonly property bool landscapeHint: overlayH < overlayW * 0.75

    background: Rectangle { color: Theme.background }

    contentItem: Item {
        width: fs.width
        height: fs.height

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                color: Theme.surface
                border.color: Theme.outline
                z: 2

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 8
                    spacing: 8

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
                        text: "Quitter"
                        onClicked: fs.close()
                    }
                }
            }

            Item {
                id: gridHost
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 8

                BingoGrid {
                    id: playGrid
                    anchors.centerIn: parent
                    availableWidth: Math.max(40, gridHost.width)
                    availableHeight: Math.max(40, gridHost.height)
                    rows: fs.rows
                    interactive: true
                    checks: fs.checks
                    bingoSet: fs.bingoSet
                    bingoRevision: fs.bingoRevision
                    gageMode: AppController.gageMode
                    gages: AppController.gages
                    onCellClicked: function(r, c) { fs.toggle(r, c) }
                }

                Label {
                    anchors.centerIn: parent
                    visible: !fs.rows || fs.rows.length === 0
                    text: "Aucune grille"
                    color: Theme.textDim
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.margins: 8
                visible: !fs.landscapeHint && gridHost.height > 0
                text: "Tournez le téléphone en paysage pour une grille plus grande"
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                color: Theme.warning
                font.pixelSize: 12
            }
        }
    }
}
