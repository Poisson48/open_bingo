import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

// Mode « Commencer la partie » — Item plein fenêtre (pas un Popup : taille fiable).
Item {
    id: fs
    anchors.fill: parent
    visible: false
    z: 10000

    property string playerName: ""
    property var rows: []
    property var checks: []
    property int playerIndex: -1

    signal checksUpdated(var newChecks)
    signal closed()

    function open() {
        visible = true
        forceActiveFocus()
        AppController.setKeepScreenOn(true)
        AppController.lockLandscape()
        AppController.setImmersive(true)
    }

    function close() {
        visible = false
        AppController.setImmersive(false)
        AppController.unlockOrientation()
        AppController.setKeepScreenOn(AppController.lastTab === 4)
        closed()
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

    // Voile plein écran
    Rectangle {
        anchors.fill: parent
        color: Theme.background
    }

    // Barre haute
    Rectangle {
        id: topBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: Math.round(Math.min(44, Math.max(36, parent.height * 0.07)))
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
            anchors.leftMargin: 12
            anchors.rightMargin: 8
            spacing: 8

            Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 40
                text: fs.playerName
                color: Theme.accent
                font.pixelSize: Math.max(14, Math.min(18, topBar.height * 0.42))
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            Label {
                text: fs.width < 520 ? String(fs.checkedCount)
                                     : (fs.checkedCount + " cochées")
                color: Theme.textDim
                font.pixelSize: 13
            }

            IconButton {
                iconName: "close"
                iconColor: Theme.text
                implicitWidth: Math.min(44, topBar.height - 4)
                implicitHeight: Math.min(44, topBar.height - 4)
                Layout.preferredWidth: implicitWidth
                Layout.preferredHeight: implicitHeight
                onClicked: fs.close()
            }
        }
    }

    // Grille : tout le reste, 0 marge morte
    BingoGrid {
        id: playGrid
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: topBar.bottom
        anchors.bottom: parent.bottom
        anchors.margins: 2
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

    // Retour système / Escape
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
            event.accepted = true
            fs.close()
        }
    }
}
