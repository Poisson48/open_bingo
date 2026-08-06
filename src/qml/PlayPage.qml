import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Page Play : aperçu + lancement plein écran (popup hors du ScrollView).
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
                    for (var r = 0; r < checks.length; r++)
                        for (var c = 0; c < checks[r].length; c++)
                            checks[r][c] = false
                    checks = checks.slice(0)
                    saveChecks()
                }

                function toggle(r, c) {
                    if (!checks[r]) return
                    checks[r] = checks[r].slice(0)
                    checks[r][c] = !checks[r][c]
                    checks = checks.slice(0)
                    saveChecks()
                    AppController.vibrate()
                }

                Component.onCompleted: reloadChecks()

                readonly property var bingoSet: {
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
                    gageMode: AppController.gageMode
                    gages: AppController.gages
                    onCellClicked: function(r, c) { playRoot.toggle(r, c) }
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

            Connections {
                target: playerBox
                function onCurrentIndexChanged() { playRoot.reloadChecks() }
            }
        }
    }

    PlayFullscreen {
        id: fullscreen
        onChecksUpdated: function(c) {
            playRoot.checks = c
            playRoot.saveChecks()
        }
    }

    function openFullscreen() {
        if (playerBox.count <= 0)
            return
        const g = AppController.grids[playerBox.currentIndex]
        fullscreen.playerName = g.player
        fullscreen.playerIndex = playerBox.currentIndex
        fullscreen.rows = g.cells
        fullscreen.checks = playRoot.checks
        fullscreen.open()
    }

    // Pour les captures / tests automatiques.
    function openFullscreenForShot() { openFullscreen() }
}
