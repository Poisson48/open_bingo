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
                model: AppController.grids
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
                    onClicked: playRoot.resetChecks()
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
                    reloadChecks()
                    const mid = Math.floor(AppController.gridSize / 2)
                    const free = AppController.freeCenter && AppController.gridSize % 2 === 1
                    for (var r = 0; r < checks.length; r++)
                        for (var c = 0; c < checks[r].length; c++)
                            checks[r][c] = free && r === mid && c === mid
                    checks = checks.slice(0)
                    bingoRevision++
                    saveChecks()
                }

                property int bingoRevision: 0
                property int lastGageR: -1
                property int lastGageC: -1

                Component.onCompleted: reloadChecks()

                function lineType(line, n) {
                    if (!line || line.length === 0) return ""
                    var rows = {}, cols = {}
                    var mainDiag = true, antiDiag = true
                    for (var i = 0; i < line.length; ++i) {
                        const r = line[i][0], c = line[i][1]
                        rows[r] = true; cols[c] = true
                        if (r !== c) mainDiag = false
                        if (r + c !== n - 1) antiDiag = false
                    }
                    if (Object.keys(rows).length === 1) return "line"
                    if (Object.keys(cols).length === 1) return "column"
                    if (mainDiag || antiDiag) return "diagonal"
                    return ""
                }

                function bingoTypes(checkGrid) {
                    var set = {}
                    const n = AppController.gridSize
                    const lines = AppController.detectBingoLines(checkGrid)
                    for (var i = 0; i < lines.length; ++i) {
                        const t = lineType(lines[i], n)
                        if (t.length) set[t] = true
                    }
                    return set
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

                readonly property var activeGageCard: {
                    void bingoRevision
                    void checks
                    void lastGageR
                    if (!AppController.gageMode || lastGageR < 0)
                        return null
                    if (!checks[lastGageR] || !checks[lastGageR][lastGageC])
                        return null
                    const grid = playerBox.count > 0
                          ? AppController.grids[playerBox.currentIndex] : null
                    if (!grid || !grid.cells) return null
                    return lookupCellGage(grid.cells[lastGageR][lastGageC])
                }

                function toggle(r, c) {
                    if (!checks[r]) return
                    const mid = Math.floor(AppController.gridSize / 2)
                    if (AppController.freeCenter && AppController.gridSize % 2 === 1
                            && r === mid && c === mid)
                        return
                    const oldTypes = bingoTypes(checks)
                    checks[r] = checks[r].slice(0)
                    checks[r][c] = !checks[r][c]
                    if (AppController.freeCenter && AppController.gridSize % 2 === 1)
                        checks[mid][mid] = true
                    checks = checks.slice(0)
                    if (checks[r][c]) {
                        lastGageR = r
                        lastGageC = c
                    } else if (lastGageR === r && lastGageC === c) {
                        lastGageR = -1
                        lastGageC = -1
                    }
                    bingoRevision++
                    saveChecks()
                    AppController.vibrate()

                    // Toast combo nouvellement déclenché (aperçu hors plein écran)
                    if (checks[r][c]) {
                        const newTypes = bingoTypes(checks)
                        const keys = ["line", "column", "diagonal"]
                        const labels = {
                            line: "Ligne", column: "Colonne", diagonal: "Diagonale"
                        }
                        for (var ki = 0; ki < keys.length; ++ki) {
                            const k = keys[ki]
                            if (newTypes[k] && !oldTypes[k]) {
                                const txt = (AppController.comboGages || {})[k] || ""
                                if (txt.length)
                                    AppController.notify(labels[k] + " — " + txt)
                            }
                        }
                    }
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
                    rows: playerBox.count > 0
                          ? AppController.grids[playerBox.currentIndex].cells : []
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
                    playRoot.reloadChecks()
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
}
