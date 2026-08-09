import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

// Page Play : aperçu + lancement plein écran (overlay Main.openPlayGame).
Item {
    id: page

    ScrollView {
        id: scroll
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            x: Theme.pad
            width: Math.max(0, scroll.availableWidth - Theme.pad * 2)
            spacing: Theme.gap

            ComboBox {
                id: playerBox
                Layout.fillWidth: true
                model: {
                    void AppController.gridsRevision
                    return AppController.grids
                }
                textRole: "player"
            }

            RowLayout {
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    text: playerBox.count > 0
                        ? "Score : " + AppController.computeScore(
                              AppController.grids[playerBox.currentIndex].player,
                              playRoot.checks) + " pts"
                        : "Générez des grilles d'abord."
                    color: Theme.text
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
                BingoButton {
                    visible: playerBox.count > 0
                    text: "Réinitialiser"
                    onClicked: confirmResetPlay.open()
                }
            }

            Item {
                id: playRoot
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(180, previewGrid.implicitHeight)
                property var checks: []

                function reloadChecks() {
                    if (playerBox.count <= 0) { checks = []; return }
                    checks = AppController.loadPlayChecks(
                        AppController.grids[playerBox.currentIndex].player)
                    if (checks.length === 0 && AppController.gridSize > 0) {
                        var empty = []
                        const mid = Math.floor(AppController.gridSize / 2)
                        for (var r = 0; r < AppController.gridSize; r++) {
                            var row = []
                            for (var c = 0; c < AppController.gridSize; c++)
                                row.push(AppController.freeCenter && AppController.gridSize % 2 === 1
                                         && r === mid && c === mid)
                            empty.push(row)
                        }
                        checks = empty
                    }
                }

                function saveChecks() {
                    if (playerBox.count <= 0) return
                    AppController.savePlayChecks(
                        AppController.grids[playerBox.currentIndex].player, checks)
                }

                function resetChecks() {
                    if (playerBox.count <= 0) return
                    AppController.resetPlayChecks("")
                    reloadChecks()
                    lastGageR = -1
                    lastGageC = -1
                    bingoRevision++
                }

                property int bingoRevision: 0
                property int lastGageR: -1
                property int lastGageC: -1
                property var previewOverlays: []

                Component.onCompleted: reloadChecks()

                readonly property var activeGageCard: {
                    void bingoRevision
                    void checks
                    void lastGageR
                    void previewOverlays
                    if (previewOverlays && previewOverlays.length > 0) {
                        const o = previewOverlays[0]
                        if (o && o.kind === "gage")
                            return { num: o.num, label: o.label, desc: o.desc }
                        if (o && o.kind === "combo")
                            return { num: 0, label: o.label || "", desc: o.desc || "" }
                    }
                    if (!AppController.gageMode || lastGageR < 0)
                        return null
                    if (!checks[lastGageR] || !checks[lastGageR][lastGageC])
                        return null
                    const grid = playerBox.count > 0
                          ? AppController.grids[playerBox.currentIndex] : null
                    if (!grid || !grid.cells) return null
                    return lookupCellGage(grid.cells[lastGageR][lastGageC])
                }

                function lookupCellGage(cell) {
                    if (!cell) return null
                    if (cell.gage && String(cell.gage).length > 0)
                        return { num: cell.points || 0, label: cell.label || "", desc: String(cell.gage) }
                    const idx = (cell.points || 0) - 1
                    const list = AppController.gages || []
                    if (idx < 0 || idx >= list.length) return null
                    const g = list[idx]
                    if (!g || !g.description) return null
                    return { num: cell.points, label: cell.label || "", desc: g.description }
                }

                function toggle(r, c) {
                    if (!checks[r] || playerBox.count <= 0) return
                    const mid = Math.floor(AppController.gridSize / 2)
                    if (AppController.freeCenter && AppController.gridSize % 2 === 1
                            && r === mid && c === mid)
                        return
                    const player = AppController.grids[playerBox.currentIndex].player
                    const result = AppController.togglePlayCell(player, r, c)
                    if (!result || !result.checks)
                        return
                    checks = result.checks
                    if (result.checked) {
                        lastGageR = r
                        lastGageC = c
                        previewOverlays = result.overlays || []
                        // Toasts pour chaque overlay (aperçu hors plein écran)
                        for (var i = 0; i < previewOverlays.length; ++i) {
                            const o = previewOverlays[i]
                            if (!o || !o.desc) continue
                            const title = o.kind === "combo"
                                          ? (o.label || "Combinaison")
                                          : ("Gage #" + (o.num || ""))
                            AppController.notify(title + " — " + o.desc)
                        }
                    } else {
                        if (lastGageR === r && lastGageC === c) {
                            lastGageR = -1
                            lastGageC = -1
                        }
                        previewOverlays = []
                    }
                    bingoRevision++
                    AppController.vibrate()
                }

                readonly property var bingoSet: {
                    void bingoRevision
                    void checks
                    var set = {}
                    const lines = AppController.detectBingoLines(checks)
                    for (var i = 0; i < lines.length; ++i) {
                        const line = lines[i]
                        for (var j = 0; j < line.length; ++j) {
                            const p = line[j]
                            set[p[0] + "," + p[1]] = true
                        }
                    }
                    return set
                }

                BingoGrid {
                    id: previewGrid
                    visible: playerBox.count > 0
                    width: playRoot.width
                    availableWidth: playRoot.width
                    // gridsRevision : forcer le rebind quand la sync / un swap
                    // change les cellules (sinon Play garde l'ancien layout).
                    rows: {
                        void AppController.gridsRevision
                        return playerBox.count > 0
                            ? AppController.grids[playerBox.currentIndex].cells : []
                    }
                    interactive: true
                    checks: playRoot.checks
                    bingoSet: playRoot.bingoSet
                    bingoRevision: playRoot.bingoRevision
                    gageMode: AppController.gageMode
                    gages: AppController.gages
                    onCellClicked: function(r, c) { playRoot.toggle(r, c) }
                }
            }

            // Carte d'action gage (aperçu) — comme l'app web
            Rectangle {
                Layout.fillWidth: true
                visible: AppController.gageMode && playerBox.count > 0
                implicitHeight: gageCardCol.implicitHeight + 20
                radius: Theme.radiusLg
                color: Theme.surface
                border.color: playRoot.activeGageCard ? Theme.accent : Theme.outline
                border.width: playRoot.activeGageCard ? 2 : 1

                ColumnLayout {
                    id: gageCardCol
                    anchors.fill: parent
                    anchors.margins: Theme.pad
                    spacing: 6
                    Label {
                        visible: !playRoot.activeGageCard
                        Layout.fillWidth: true
                        text: "Coche une case pour voir le gage à effectuer"
                        color: Theme.textDim
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                    }
                    Label {
                        visible: !!playRoot.activeGageCard
                        text: playRoot.activeGageCard
                              ? ("Gage #" + playRoot.activeGageCard.num)
                              : ""
                        color: Theme.accent
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }
                    Label {
                        visible: !!playRoot.activeGageCard
                        Layout.fillWidth: true
                        text: playRoot.activeGageCard ? playRoot.activeGageCard.label : ""
                        color: Theme.textDim
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }
                    Label {
                        visible: !!playRoot.activeGageCard
                        Layout.fillWidth: true
                        text: playRoot.activeGageCard ? playRoot.activeGageCard.desc : ""
                        color: Theme.text
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                visible: playerBox.count > 0
                implicitHeight: rulesCol.implicitHeight + 20
                radius: Theme.radiusLg
                color: Theme.surface
                border.color: Theme.outline
                ColumnLayout {
                    id: rulesCol
                    anchors.fill: parent
                    anchors.margins: Theme.pad
                    spacing: 6
                    Label {
                        text: AppController.gageMode ? "Gages de combinaison" : "Multiplicateurs"
                        color: Theme.text
                        font.weight: Font.DemiBold
                    }
                    Repeater {
                        model: AppController.gageMode
                               ? [
                                   { k: "line", l: "↔ Ligne", v: AppController.comboGages.line },
                                   { k: "column", l: "↕ Colonne", v: AppController.comboGages.column },
                                   { k: "diagonal", l: "⤡ Diagonale", v: AppController.comboGages.diagonal }
                                 ]
                               : [
                                   { k: "line", l: "Ligne", v: "× " + AppController.multipliers.line },
                                   { k: "column", l: "Colonne", v: "× " + AppController.multipliers.column },
                                   { k: "diagonal", l: "Diagonale", v: "× " + AppController.multipliers.diagonal },
                                   { k: "full", l: "Grille complète", v: "× " + AppController.multipliers.full }
                                 ]
                        Label {
                            Layout.fillWidth: true
                            visible: modelData.v && String(modelData.v).length > 0
                            text: modelData.l + " — " + modelData.v
                            color: Theme.textDim
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                visible: playerBox.count > 0 && playRoot.width < playRoot.height
                text: "Astuce : lancez la partie en plein écran (paysage) pour cocher confortablement."
                wrapMode: Text.WordWrap
                color: Theme.textDim
                font.pixelSize: 12
            }

            BingoButton {
                Layout.fillWidth: true
                visible: playerBox.count > 0
                text: "Commencer la partie (plein écran)"
                primary: true
                onClicked: page.openFullscreen()
            }

            // Marge au-dessus de la barre de geste Android.
            Item { Layout.preferredHeight: Theme.pad * 2 }

            Connections {
                target: playerBox
                function onCurrentIndexChanged() {
                    playRoot.lastGageR = -1
                    playRoot.lastGageC = -1
                    playRoot.previewOverlays = []
                    playRoot.reloadChecks()
                }
            }
            Connections {
                target: AppController
                function onPlayChecksChanged() {
                    playRoot.reloadChecks()
                    playRoot.bingoRevision++
                }
            }
        }
    }

    function applyPlayChecks(c) {
        playRoot.checks = c
        playRoot.saveChecks()
    }

    function openFullscreen() {
        if (playerBox.count <= 0)
            return
        const g = AppController.grids[playerBox.currentIndex]
        const w = page.Window.window
        if (w && typeof w.openPlayGame === "function") {
            w.openPlayGame(g.player, playerBox.currentIndex, g.cells, playRoot.checks)
            return
        }
        console.warn("openPlayGame indisponible — overlay Main manquant")
    }

    function openFullscreenForShot() { openFullscreen() }

    ColoDialog {
        id: confirmResetPlay
        title: "Réinitialiser la partie ?"
        destructive: true
        acceptText: "Réinitialiser"
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            color: Theme.text
            text: "Effacer toutes les cases cochées de tous les joueurs ? "
                  + "Cette action est synchronisée avec les autres appareils."
        }
        onAccepted: playRoot.resetChecks()
    }
}
