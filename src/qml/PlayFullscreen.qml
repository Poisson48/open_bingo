import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

// Mode « Commencer la partie » — Item plein fenêtre (pas un Popup : taille fiable).
// Overlay gage (bas, 15 s) comme l'app web : action de case + gages de combinaison.
Item {
    id: fs
    anchors.fill: parent
    visible: false
    z: 10000

    property string playerName: ""
    property var rows: []
    property var checks: []
    property int playerIndex: -1

    // Overlay temporaire
    property bool gagePanelVisible: false
    property var activeGage: null      // { num, label, desc } ou null
    property var activeCombos: []      // ["line"|"column"|"diagonal", ...]
    readonly property int gageDisplayMs: 15000

    signal checksUpdated(var newChecks)
    signal closed()

    function open() {
        visible = true
        clearGagePanel()
        forceActiveFocus()
        AppController.setKeepScreenOn(true)
        AppController.lockLandscape()
        AppController.setImmersive(true)
    }

    function close() {
        clearGagePanel()
        visible = false
        AppController.setImmersive(false)
        AppController.unlockOrientation()
        AppController.setKeepScreenOn(AppController.lastTab === 4)
        closed()
    }

    function clearGagePanel() {
        gageHideTimer.stop()
        gagePanelVisible = false
        activeGage = null
        activeCombos = []
        timerBarAnim.stop()
        timerBar.width = timerTrack.width
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

    function lineType(line, n) {
        if (!line || line.length === 0)
            return ""
        var rows = {}
        var cols = {}
        var mainDiag = true
        var antiDiag = true
        for (var i = 0; i < line.length; ++i) {
            const r = line[i][0]
            const c = line[i][1]
            rows[r] = true
            cols[c] = true
            if (r !== c)
                mainDiag = false
            if (r + c !== n - 1)
                antiDiag = false
        }
        if (Object.keys(rows).length === 1)
            return "line"
        if (Object.keys(cols).length === 1)
            return "column"
        if (mainDiag || antiDiag)
            return "diagonal"
        return ""
    }

    function bingoTypes(checkGrid) {
        var set = {}
        const n = AppController.gridSize
        const lines = AppController.detectBingoLines(checkGrid)
        for (var i = 0; i < lines.length; ++i) {
            const t = lineType(lines[i], n)
            if (t.length)
                set[t] = true
        }
        return set
    }

    function lookupCellGage(cell) {
        if (!cell)
            return null
        if (cell.gage && String(cell.gage).length > 0)
            return {
                num: cell.points || 0,
                label: cell.label || "",
                desc: String(cell.gage)
            }
        const idx = (cell.points || 0) - 1
        const list = AppController.gages || []
        if (idx < 0 || idx >= list.length)
            return null
        const g = list[idx]
        if (!g || !g.description)
            return null
        return {
            num: cell.points,
            label: cell.label || "",
            desc: g.description
        }
    }

    function comboLabel(key) {
        if (key === "line") return "Ligne complète"
        if (key === "column") return "Colonne complète"
        if (key === "diagonal") return "Diagonale complète"
        return key
    }

    function comboIcon(key) {
        if (key === "line") return "↔"
        if (key === "column") return "↕"
        if (key === "diagonal") return "⤡"
        return "★"
    }

    function comboText(key) {
        const c = AppController.comboGages || {}
        return c[key] || ""
    }

    function showGagePanel(gageData, newCombos) {
        const combos = []
        for (var i = 0; i < newCombos.length; ++i) {
            const k = newCombos[i]
            if (comboText(k).length > 0)
                combos.push(k)
        }
        if (!gageData && combos.length === 0) {
            clearGagePanel()
            return
        }
        activeGage = gageData
        activeCombos = combos
        gagePanelVisible = true
        gageHideTimer.restart()
        Qt.callLater(function() {
            timerBarAnim.stop()
            timerBar.width = Math.max(1, timerTrack.width)
            timerBarAnim.start()
        })
    }

    function toggle(r, c) {
        if (!checks || !checks[r])
            return
        const mid = Math.floor(AppController.gridSize / 2)
        if (AppController.freeCenter && AppController.gridSize % 2 === 1
                && r === mid && c === mid)
            return

        const oldTypes = bingoTypes(checks)

        var copy = deepCopyChecks(checks)
        copy[r][c] = !copy[r][c]
        if (AppController.freeCenter && AppController.gridSize % 2 === 1
                && mid < copy.length && mid < (copy[mid] || []).length)
            copy[mid][mid] = true
        checks = copy
        bingoRevision++
        checksUpdated(copy)
        AppController.vibrate()

        // Gages : uniquement à la cochage (pas au décochage)
        if (!copy[r][c]) {
            clearGagePanel()
            return
        }

        const newTypes = bingoTypes(copy)
        var newCombos = []
        const keys = ["line", "column", "diagonal"]
        for (var ki = 0; ki < keys.length; ++ki) {
            const k = keys[ki]
            if (newTypes[k] && !oldTypes[k])
                newCombos.push(k)
        }

        var gageData = null
        if (AppController.gageMode && fs.rows && fs.rows[r])
            gageData = lookupCellGage(fs.rows[r][c])

        showGagePanel(gageData, newCombos)
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

    Timer {
        id: gageHideTimer
        interval: fs.gageDisplayMs
        repeat: false
        onTriggered: clearGagePanel()
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

    // Grille : tout le reste
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

    // Overlay gage / combo — glisse depuis le bas, 15 s (parité app web)
    Rectangle {
        id: gagePanel
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        z: 20
        visible: fs.gagePanelVisible
        color: Qt.rgba(0.04, 0.06, 0.11, 0.94)
        border.color: Theme.accent
        border.width: 0
        implicitHeight: gageCol.implicitHeight + 28
        height: Math.min(implicitHeight, parent.height * 0.65)

        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 2
            color: Theme.accent
        }

        ColumnLayout {
            id: gageCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 14
            anchors.bottomMargin: 10
            spacing: 10

            // Gage de la case cochée
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                visible: fs.activeGage !== null

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Label {
                        text: fs.activeGage ? ("Gage #" + fs.activeGage.num) : ""
                        color: Theme.accent
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }
                    Label {
                        Layout.fillWidth: true
                        text: fs.activeGage ? fs.activeGage.label : ""
                        color: Theme.textDim
                        font.pixelSize: 13
                        elide: Text.ElideRight
                        maximumLineCount: 2
                        wrapMode: Text.WordWrap
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: fs.activeGage ? fs.activeGage.desc : ""
                    color: Theme.text
                    font.pixelSize: Math.max(16, Math.min(22, fs.height * 0.035))
                    font.weight: Font.DemiBold
                    wrapMode: Text.WordWrap
                }
            }

            // Gages de combinaison nouvellement déclenchés
            Repeater {
                model: fs.activeCombos
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        visible: index > 0 || fs.activeGage !== null
                        color: Qt.rgba(1, 1, 1, 0.12)
                    }
                    Label {
                        text: fs.comboIcon(modelData) + "  " + fs.comboLabel(modelData)
                        color: Theme.success
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }
                    Label {
                        Layout.fillWidth: true
                        text: fs.comboText(modelData)
                        color: Theme.text
                        font.pixelSize: Math.max(15, Math.min(20, fs.height * 0.032))
                        font.weight: Font.DemiBold
                        wrapMode: Text.WordWrap
                    }
                }
            }

            // Barre de décompte 15 s
            Item {
                id: timerTrack
                Layout.fillWidth: true
                Layout.topMargin: 4
                height: 3
                Rectangle {
                    anchors.fill: parent
                    radius: 2
                    color: Qt.rgba(1, 1, 1, 0.15)
                }
                Rectangle {
                    id: timerBar
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: parent.width
                    radius: 2
                    color: Theme.accent
                }
                NumberAnimation {
                    id: timerBarAnim
                    target: timerBar
                    property: "width"
                    to: 0
                    duration: fs.gageDisplayMs
                    easing.type: Easing.Linear
                }
            }
        }

        // Entrée légère
        opacity: visible ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 180 } }
    }

    // Retour système / Escape
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
            event.accepted = true
            if (fs.gagePanelVisible) {
                clearGagePanel()
                return
            }
            fs.close()
        }
    }
}
