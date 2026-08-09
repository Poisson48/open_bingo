import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

// Mode « Commencer la partie » — Item plein fenêtre.
// Cochage via AppController.togglePlayCell (pattern Colo Courses/Tâches) :
// même libellé → coché chez tous les joueurs. Overlays gages en file (15 s).
Item {
    id: fs
    anchors.fill: parent
    visible: false
    z: 10000

    property string playerName: ""
    property var rows: []
    property var checks: []
    property int playerIndex: -1

    property bool gagePanelVisible: false
    property var activeOverlay: null   // { kind, num, label, desc, player, key? }
    property var gageQueue: []
    readonly property int gageDisplayMs: 15000
    readonly property int queueRemaining: gageQueue.length

    signal checksUpdated(var newChecks)
    signal closed()

    function open() {
        visible = true
        clearGagePanel()
        hideWinner()
        reloadRowsFromController()
        reloadChecksFromController()
        forceActiveFocus()
        AppController.setKeepScreenOn(true)
        AppController.lockLandscape()
        AppController.setImmersive(true)
    }

    function close() {
        clearGagePanel()
        hideWinner()
        visible = false
        AppController.setImmersive(false)
        AppController.unlockOrientation()
        AppController.setKeepScreenOn(AppController.lastTab === 5)
        closed()
    }

    function reloadRowsFromController() {
        if (!playerName || playerName.length === 0)
            return
        const grids = AppController.grids
        for (var i = 0; i < grids.length; ++i) {
            if (grids[i].player === playerName) {
                rows = grids[i].cells || []
                playerIndex = i
                bingoRevision++
                return
            }
        }
    }

    // Comme PlayPage : recharger depuis la DB quand la sync distante (ou un
    // autre joueur local) met à jour les coches — sinon le plein écran reste
    // figé sur sa copie initiale.
    function reloadChecksFromController() {
        if (!playerName || playerName.length === 0)
            return
        var loaded = AppController.loadPlayChecks(playerName)
        if (!loaded || loaded.length === 0) {
            const n = AppController.gridSize
            if (n <= 0) {
                checks = []
                bingoRevision++
                return
            }
            var empty = []
            const mid = Math.floor(n / 2)
            for (var r = 0; r < n; r++) {
                var row = []
                for (var c = 0; c < n; c++)
                    row.push(AppController.freeCenter && n % 2 === 1 && r === mid && c === mid)
                empty.push(row)
            }
            loaded = empty
        }
        checks = loaded
        bingoRevision++
        // Pas de checksUpdated ici : ça rappellerait savePlayChecks → republish.
        // PlayPage écoute déjà playChecksChanged pour se rafraîchir.
    }

    Connections {
        target: AppController
        enabled: fs.visible
        function onPlayChecksChanged() {
            fs.reloadChecksFromController()
        }
        function onGridsChanged() {
            fs.reloadRowsFromController()
            fs.reloadChecksFromController()
        }
    }

    function clearGagePanel() {
        gageHideTimer.stop()
        gagePanelVisible = false
        activeOverlay = null
        gageQueue = []
        timerBarAnim.stop()
        timerBar.width = timerTrack.width
    }

    function enqueueOverlays(list) {
        if (!list || list.length === 0)
            return
        var q = gageQueue.slice(0)
        for (var i = 0; i < list.length; ++i)
            q.push(list[i])
        gageQueue = q
        if (!gagePanelVisible)
            showNextOverlay()
    }

    function showNextOverlay() {
        if (!gageQueue || gageQueue.length === 0) {
            clearGagePanel()
            return
        }
        activeOverlay = gageQueue[0]
        gageQueue = gageQueue.slice(1)
        gagePanelVisible = !!activeOverlay
        if (!gagePanelVisible) {
            showNextOverlay()
            return
        }
        gageHideTimer.restart()
        Qt.callLater(function() {
            timerBarAnim.stop()
            timerBar.width = Math.max(1, timerTrack.width)
            timerBarAnim.start()
        })
    }

    function dismissCurrentOverlay() {
        gageHideTimer.stop()
        timerBarAnim.stop()
        if (gageQueue && gageQueue.length > 0)
            showNextOverlay()
        else
            clearGagePanel()
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

    function toggle(r, c) {
        if (!checks || !checks[r])
            return
        const mid = Math.floor(AppController.gridSize / 2)
        if (AppController.freeCenter && AppController.gridSize % 2 === 1
                && r === mid && c === mid)
            return

        const result = AppController.togglePlayCell(fs.playerName, r, c)
        if (!result || !result.checks)
            return

        checks = result.checks
        bingoRevision++
        checksUpdated(result.checks)
        AppController.vibrate()

        // Gagnant : grille pleine (soi ou un autre via sync libellé).
        // Décocher annule l'annonce si plus personne n'a une grille pleine.
        const winners = result.winners || []
        if (!result.checked) {
            clearGagePanel()
            if (!winners.length)
                hideWinner()
            return
        }
        if (result.justCompleted && result.newWinners && result.newWinners.length > 0)
            showWinner(result.newWinners, result.scoreboard || [])
        enqueueOverlays(result.overlays || [])
    }

    property bool winnerVisible: false
    property var winnerEntries: []   // [{player, score, unit, ...}]
    property var winnerBoard: []

    function showWinner(entries, board) {
        winnerEntries = entries || []
        winnerBoard = board || []
        winnerVisible = winnerEntries.length > 0
        if (winnerVisible)
            clearGagePanel()
    }

    function hideWinner() {
        winnerVisible = false
        winnerEntries = []
        winnerBoard = []
    }

    readonly property string winnerHeadline: {
        if (!winnerEntries || winnerEntries.length === 0)
            return ""
        if (winnerEntries.length === 1)
            return (winnerEntries[0].player || "") + " a gagné !"
        var names = []
        for (var i = 0; i < winnerEntries.length; ++i)
            names.push(winnerEntries[i].player || "")
        return names.join(", ") + " ont gagné !"
    }

    readonly property string winnerSubline: {
        if (!winnerEntries || winnerEntries.length === 0)
            return "Grille complète"
        const e = winnerEntries[0]
        const unit = e.unit || "pts"
        if (winnerEntries.length === 1)
            return "Grille complète — " + Number(e.score || 0).toLocaleString(Qt.locale("fr_FR")) + " " + unit
        return winnerEntries.length + " grilles complètes"
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

    readonly property bool overlayIsCombo: activeOverlay && activeOverlay.kind === "combo"
    readonly property string overlayTitle: {
        if (!activeOverlay) return ""
        if (overlayIsCombo)
            return comboIcon(activeOverlay.key) + "  " + (activeOverlay.label || comboLabel(activeOverlay.key))
        return "Gage #" + (activeOverlay.num || 0)
    }

    Timer {
        id: gageHideTimer
        interval: fs.gageDisplayMs
        repeat: false
        onTriggered: fs.dismissCurrentOverlay()
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.background
    }

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

    Rectangle {
        id: gagePanel
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        z: 20
        visible: fs.gagePanelVisible
        color: Qt.rgba(0.04, 0.06, 0.11, 0.94)
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

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Label {
                    Layout.fillWidth: true
                    text: fs.overlayTitle
                    color: fs.overlayIsCombo ? Theme.success : Theme.accent
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
                Label {
                    visible: fs.queueRemaining > 0
                    text: "+" + fs.queueRemaining
                    color: Theme.textDim
                    font.pixelSize: 12
                }
            }

            Label {
                Layout.fillWidth: true
                visible: !!(fs.activeOverlay && fs.activeOverlay.label && !fs.overlayIsCombo)
                text: fs.activeOverlay ? (fs.activeOverlay.label || "") : ""
                color: Theme.textDim
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                text: fs.activeOverlay ? (fs.activeOverlay.desc || "") : ""
                color: Theme.text
                font.pixelSize: Math.max(16, Math.min(22, fs.height * 0.035))
                font.weight: Font.DemiBold
                wrapMode: Text.WordWrap
            }

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

        MouseArea {
            anchors.fill: parent
            onClicked: fs.dismissCurrentOverlay()
        }

        opacity: visible ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 180 } }
    }

    Rectangle {
        id: winnerPanel
        anchors.fill: parent
        z: 50
        visible: fs.winnerVisible
        color: Qt.rgba(0.04, 0.06, 0.11, 0.88)

        MouseArea {
            anchors.fill: parent
            onClicked: fs.hideWinner()
        }

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width - 48, 480)
            implicitHeight: winCol.implicitHeight + 40
            radius: Theme.radiusLg
            color: Theme.surface
            border.color: Theme.success
            border.width: 2

            ColumnLayout {
                id: winCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 20
                spacing: 12

                Label {
                    Layout.fillWidth: true
                    text: "BINGO !"
                    color: Theme.success
                    font.pixelSize: 28
                    font.weight: Font.Black
                    horizontalAlignment: Text.AlignHCenter
                }
                Label {
                    Layout.fillWidth: true
                    text: fs.winnerHeadline
                    color: Theme.text
                    font.pixelSize: 20
                    font.weight: Font.DemiBold
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }
                Label {
                    Layout.fillWidth: true
                    text: fs.winnerSubline
                    color: Theme.textDim
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }

                Repeater {
                    model: Math.min(5, (fs.winnerBoard || []).length)
                    delegate: Label {
                        Layout.fillWidth: true
                        required property int index
                        readonly property var entry: fs.winnerBoard[index]
                        text: (index + 1) + ". " + (entry.player || "")
                              + " — "
                              + Number(entry.score || 0).toLocaleString(Qt.locale("fr_FR"))
                              + " " + (entry.unit || "pts")
                              + (entry.full ? "  ✓" : "")
                        color: entry.full ? Theme.success : Theme.textDim
                        font.pixelSize: 13
                        font.weight: index === 0 ? Font.DemiBold : Font.Normal
                        elide: Text.ElideRight
                    }
                }

                BingoButton {
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    text: "Continuer"
                    primary: true
                    onClicked: fs.hideWinner()
                }
                BingoButton {
                    Layout.fillWidth: true
                    text: "Voir les scores"
                    onClicked: {
                        fs.hideWinner()
                        fs.close()
                        AppController.lastTab = 6
                    }
                }
            }
        }

        opacity: visible ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 200 } }
    }

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
            event.accepted = true
            if (fs.winnerVisible) {
                fs.hideWinner()
                return
            }
            if (fs.gagePanelVisible) {
                fs.dismissCurrentOverlay()
                return
            }
            fs.close()
        }
    }
}
