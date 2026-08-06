import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Grille bingo responsive.
// - interactive : cocher (Play)
// - editable : glisser pour échanger / cliquer pour remplacer (Grilles)
// Avec availableHeight > 0 : cases carrées calées dans l'espace (plein écran).
Column {
    id: root
    property var rows: []
    property real availableWidth: 320
    property real availableHeight: 0
    property bool interactive: false
    property bool editable: false
    property int playerIndex: -1
    property var checks: null
    property var bingoSet: ({})
    property int bingoRevision: 0
    property bool gageMode: false
    property var gages: []
    // En play plein écran : masquer les points pour laisser la place au texte.
    property bool hidePoints: false

    signal cellClicked(int row, int col)
    signal cellEditRequested(int row, int col, string label)

    spacing: gap
    width: gridPixelW
    clip: true

    readonly property int n: rows && rows.length ? rows.length : 0
    readonly property int cols: {
        if (n <= 0 || !rows[0])
            return 0
        const row0 = rows[0]
        return row0.length !== undefined ? row0.length : 0
    }
    readonly property real gap: {
        const m = Math.min(availableWidth, availableHeight > 0 ? availableHeight : availableWidth)
        if (m >= 500) return 4
        if (m >= 320) return 3
        return 2
    }

    // Cases carrées qui tiennent dans largeur × hauteur disponibles.
    readonly property real cellSide: {
        if (cols <= 0 || n <= 0)
            return 48
        const gapW = gap * (cols - 1)
        const gapH = gap * (n - 1)
        const maxW = Math.max(20, availableWidth - gapW) / cols
        if (availableHeight <= 0)
            return Math.max(24, Math.floor(maxW))
        const maxH = Math.max(20, availableHeight - gapH) / n
        return Math.max(22, Math.floor(Math.min(maxW, maxH)))
    }
    readonly property real cellW: cellSide
    readonly property real cellH: cellSide
    readonly property real gridPixelW: cols > 0 ? cols * cellSide + gap * (cols - 1) : availableWidth
    readonly property real gridPixelH: n > 0 ? n * cellSide + gap * (n - 1) : cellSide

    property int dragFromRow: -1
    property int dragFromCol: -1
    property int dragOverRow: -1
    property int dragOverCol: -1
    property bool didDrag: false
    readonly property real dragThreshold: 10

    // Une seule taille pour toute la grille = rendu homogène (calée sur le pire texte).
    readonly property real uniformFontSize: {
        const side = Math.min(cellW, cellH)
        if (side <= 0 || n <= 0)
            return 10

        let maxLen = 4
        let maxWord = 4
        for (let r = 0; r < n; ++r) {
            const row = rows[r]
            if (!row) continue
            const rowLen = row.length !== undefined ? row.length : 0
            for (let c = 0; c < rowLen; ++c) {
                const cell = row[c]
                if (!cell || cell.isFree) continue
                const lab = cell.label || ""
                maxLen = Math.max(maxLen, lab.length)
                maxWord = Math.max(maxWord, longestWordLen(lab))
            }
        }

        const pad = Math.max(2, side * 0.04)
        const usableW = Math.max(10, cellW - pad * 2)
        const usableH = Math.max(10, cellH - pad * 2)
        const glyph = 0.68
        const lineFactor = 1.22

        const maxFsz = Math.max(9, Math.floor(side * 0.26))
        for (let fsz = maxFsz; fsz >= 7; --fsz) {
            const charsPerLine = Math.max(2, Math.floor(usableW / (fsz * glyph)))
            if (charsPerLine < maxWord)
                continue
            const maxLines = Math.max(1, Math.floor(usableH / (fsz * lineFactor)))
            if (charsPerLine * maxLines >= Math.ceil(maxLen * 1.2))
                return fsz
        }
        return 7
    }

    function cellFontSize(label, free) {
        const side = Math.min(cellW, cellH)
        if (free)
            return Math.max(uniformFontSize, Math.min(Math.floor(side * 0.28), uniformFontSize + 4))
        return uniformFontSize
    }

    function maxLabelLines(fsz) {
        const pad = Math.max(2, Math.min(cellW, cellH) * 0.04)
        const usableH = cellH - pad * 2
        const lineH = Math.max(1, fsz * 1.22)
        return Math.max(1, Math.min(10, Math.floor(usableH / lineH)))
    }

    function longestWordLen(text) {
        if (!text || !text.length)
            return 1
        const parts = String(text).split(/\s+/)
        let m = 1
        for (let i = 0; i < parts.length; ++i)
            m = Math.max(m, parts[i].length)
        return m
    }

    Repeater {
        model: root.n
        Row {
            spacing: root.gap
            property int rowIndex: index
            Repeater {
                model: root.cols
                Rectangle {
                    id: cellRect
                    width: root.cellW
                    height: root.cellH
                    radius: Math.max(2, Math.min(Theme.radius, root.cellSide * 0.12))
                    clip: true
                    property int colIndex: index
                    property var cell: root.rows[rowIndex][colIndex]
                    property bool isFree: cell && cell.isFree
                    property bool checked: {
                        if (isFree)
                            return true
                        return !!(root.checks && root.checks[rowIndex]
                                  && root.checks[rowIndex][colIndex])
                    }
                    property bool inBingo: {
                        void root.bingoRevision
                        return root.bingoSet[rowIndex + "," + colIndex] === true
                    }
                    property bool marked: isFree || checked
                    property string cellLabel: cell && cell.label ? cell.label : ""
                    property real labelSize: root.cellFontSize(cellLabel, isFree)
                    property real cellPad: Math.max(2, root.cellSide * 0.06)
                    property bool isDragSrc: root.dragFromRow === rowIndex
                                            && root.dragFromCol === colIndex
                    property bool isDragOver: root.dragOverRow === rowIndex
                                             && root.dragOverCol === colIndex
                                             && !isDragSrc

                    color: isDragSrc ? Theme.surfaceHigh
                         : isDragOver ? Theme.accentSoft
                         : inBingo ? Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.35)
                         : marked ? Theme.accentSoft : Theme.inputBg
                    border.color: isDragOver ? Theme.accent
                                : inBingo ? Theme.success
                                : marked ? Theme.accent : Theme.outline
                    border.width: (isDragOver || inBingo) ? 2 : 1
                    opacity: isDragSrc && root.didDrag ? 0.45 : 1

                    Column {
                        anchors.fill: parent
                        anchors.margins: cellPad
                        spacing: 0

                        Text {
                            id: labelText
                            width: parent.width
                            height: parent.height - (pointsText.visible ? pointsText.implicitHeight : 0)
                            text: isFree ? "FREE" : cellLabel
                            color: isFree ? Theme.accent : Theme.text
                            font.pixelSize: labelSize
                            font.weight: isFree ? Font.Bold : Font.Normal
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            wrapMode: Text.WordWrap
                            // Pas d'ellipse ni coupure au milieu d'un mot : police adaptée.
                            elide: Text.ElideNone
                            maximumLineCount: root.maxLabelLines(labelSize)
                            clip: true
                            opacity: checked && !isFree ? 0.92 : 1
                        }
                        Text {
                            id: pointsText
                            width: parent.width
                            visible: !root.hidePoints && !isFree && cell
                                     && root.cellSide >= 40
                            text: {
                                if (root.gageMode && cell.gage)
                                    return "Gage #" + cell.points
                                if (cell.points !== undefined)
                                    return cell.points + " pt"
                                return ""
                            }
                            color: root.gageMode ? Theme.accent : Theme.textDim
                            font.pixelSize: Math.max(6, Math.min(labelSize * 0.7, root.cellSide * 0.14))
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideRight
                            maximumLineCount: 1
                            clip: true
                        }
                    }

                    // Coche en coin : ne masque plus le texte au centre.
                    Label {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: Math.max(1, root.cellSide * 0.04)
                        visible: checked && !isFree
                        text: "✓"
                        color: inBingo ? Theme.success : Theme.accent
                        font.pixelSize: Math.max(10, Math.min(root.cellSide * 0.28, 22))
                        font.weight: Font.Bold
                        opacity: 0.95
                    }

                    MouseArea {
                        anchors.fill: parent
                        enabled: !isFree && (root.interactive || root.editable)
                        preventStealing: root.editable
                        property real startX: 0
                        property real startY: 0

                        onPressed: function(mouse) {
                            if (!root.editable) return
                            startX = mouse.x
                            startY = mouse.y
                            root.didDrag = false
                            root.dragFromRow = rowIndex
                            root.dragFromCol = colIndex
                            root.dragOverRow = -1
                            root.dragOverCol = -1
                        }
                        onPositionChanged: function(mouse) {
                            if (!root.editable || root.dragFromRow < 0) return
                            const dx = mouse.x - startX
                            const dy = mouse.y - startY
                            if (!root.didDrag && Math.hypot(dx, dy) > root.dragThreshold)
                                root.didDrag = true
                            if (!root.didDrag) return
                            const global = cellRect.mapToItem(root, mouse.x, mouse.y)
                            const r = Math.floor(global.y / (root.cellH + root.gap))
                            const c = Math.floor(global.x / (root.cellW + root.gap))
                            if (r >= 0 && c >= 0 && r < root.n && c < root.cols
                                    && !(r === root.dragFromRow && c === root.dragFromCol)) {
                                const dest = root.rows[r][c]
                                if (dest && !dest.isFree) {
                                    root.dragOverRow = r
                                    root.dragOverCol = c
                                    return
                                }
                            }
                            root.dragOverRow = -1
                            root.dragOverCol = -1
                        }
                        onReleased: function(mouse) {
                            if (root.editable && root.dragFromRow >= 0) {
                                if (root.didDrag && root.dragOverRow >= 0
                                        && root.playerIndex >= 0) {
                                    AppController.swapGridCells(
                                        root.playerIndex,
                                        root.dragFromRow, root.dragFromCol,
                                        root.dragOverRow, root.dragOverCol)
                                } else if (!root.didDrag) {
                                    root.cellEditRequested(rowIndex, colIndex, cellLabel)
                                }
                            } else if (root.interactive) {
                                root.cellClicked(rowIndex, colIndex)
                            }
                            root.dragFromRow = -1
                            root.dragFromCol = -1
                            root.dragOverRow = -1
                            root.dragOverCol = -1
                            root.didDrag = false
                        }
                        onCanceled: {
                            root.dragFromRow = -1
                            root.dragFromCol = -1
                            root.dragOverRow = -1
                            root.dragOverCol = -1
                            root.didDrag = false
                        }
                    }
                }
            }
        }
    }
}
