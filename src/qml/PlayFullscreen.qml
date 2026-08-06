import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

// Play plein écran — ancré sur Overlay (pas de x/y manuels qui ratent la taille).
Popup {
    id: fs
    property string playerName: ""
    property var rows: []
    property var checks: []
    property int playerIndex: -1

    signal checksUpdated(var newChecks)

    parent: Overlay.overlay
    modal: true
    dim: false
    focus: true
    padding: 0
    margins: 0
    closePolicy: Popup.NoAutoClose

    // Popup Qt 6.4 n'accepte pas anchors.fill — lier explicitement à l'overlay.
    x: 0
    y: 0
    width: Overlay.overlay ? Overlay.overlay.width : Screen.width
    height: Overlay.overlay ? Overlay.overlay.height : Screen.height

    onOpened: {
        AppController.setKeepScreenOn(true)
        AppController.lockLandscape()
        AppController.setImmersive(true)
    }
    onClosed: {
        AppController.setImmersive(false)
        AppController.unlockOrientation()
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

    background: Rectangle { color: Theme.background; anchors.fill: parent }

    contentItem: Item {
        anchors.fill: parent
        clip: true

        Rectangle {
            id: topBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: Math.round(Math.min(40, Math.max(34, parent.height * 0.08)))
            color: Theme.surface
            z: 2

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.outline
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 12
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 48
                    text: fs.playerName
                    color: Theme.accent
                    font.pixelSize: Math.max(13, Math.min(17, topBar.height * 0.45))
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }

                Label {
                    text: fs.width < 520 ? String(fs.checkedCount)
                                         : (fs.checkedCount + " cochées")
                    color: Theme.textDim
                    font.pixelSize: 12
                    Layout.alignment: Qt.AlignVCenter
                }

                IconButton {
                    iconName: "close"
                    iconColor: Theme.text
                    implicitWidth: 40
                    implicitHeight: 40
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    Layout.alignment: Qt.AlignVCenter
                    Layout.rightMargin: 4
                    onClicked: fs.close()
                }
            }
        }

        // Grille : 100 % de la largeur et de la hauteur restantes.
        BingoGrid {
            id: playGrid
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: topBar.bottom
            anchors.bottom: parent.bottom
            anchors.margins: 3
            availableWidth: width
            availableHeight: height
            fillBounds: true
            rows: fs.rows
            interactive: true
            hidePoints: true
            checks: fs.checks
            bingoSet: fs.bingoSet
            bingoRevision: fs.bingoRevision
            gageMode: AppController.gageMode
            gages: AppController.gages
            onCellClicked: function(r, c) { fs.toggle(r, c) }
        }

        Label {
            anchors.centerIn: playGrid
            visible: !fs.rows || fs.rows.length === 0
            text: "Aucune grille"
            color: Theme.textDim
            z: 1
        }
    }
}
