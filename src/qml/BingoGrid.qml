import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Grille bingo responsive.
// fillBounds=true : GridLayout qui étire chaque case pour remplir le parent.
Item {
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
    property bool hidePoints: false
    property bool fillBounds: false

    signal cellClicked(int row, int col)
    signal cellEditRequested(int row, int col, string label)

    readonly property int n: rows && rows.length ? rows.length : 0
    readonly property int cols: {
        if (n <= 0 || !rows[0])
            return 0
        const row0 = rows[0]
        return row0.length !== undefined ? row0.length : 0
    }
    readonly property real gap: {
        const m = Math.min(
            availableWidth,
            availableHeight > 0 ? availableHeight : availableWidth)
        if (m >= 700) return 4
        if (m >= 400) return 3
        return 2
    }

    readonly property real cellW: {
        if (cols <= 0) return 48
        if (fillBounds && width > 0)
            return Math.max(18, (width - gap * (cols - 1)) / cols)
        return Math.max(18, Math.floor((availableWidth - gap * (cols - 1)) / cols))
    }
    readonly property real cellH: {
        if (n <= 0) return 48
        if (fillBounds && height > 0)
            return Math.max(18, (height - gap * (n - 1)) / n)
        if (availableHeight > 0)
            return Math.min(cellW, Math.max(18, Math.floor((availableHeight - gap * (n - 1)) / n)))
        return Math.max(28, Math.floor(cellW * 0.82))
    }
    readonly property real cellSide: Math.min(cellW, cellH)
    readonly property real gridPixelW: cols > 0 ? cols * cellW + gap * (cols - 1) : availableWidth
    readonly property real gridPixelH: n > 0 ? n * cellH + gap * (n - 1) : cellH

    // Hors fillBounds : taille intrinsèque pour ScrollView / cartes.
    // En fillBounds : ne pas lier implicit* à width/height (boucle de binding
    // après rotation Android → taille incorrecte / crop).
    implicitWidth: fillBounds ? 0 : gridPixelW
    implicitHeight: fillBounds ? 0 : gridPixelH

    property int dragFromRow: -1
    property int dragFromCol: -1
    property int dragOverRow: -1
    property int dragOverCol: -1
    property bool didDrag: false
    readonly property real dragThreshold: 10

    readonly property real uniformFontSize: {
        const cw = cellW
        const ch = cellH
        if (cw <= 0 || ch <= 0 || n <= 0)
            return 12

        let maxWord = 4
        let maxLen = 4
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

        const pad = Math.max(2, Math.min(cw, ch) * 0.05)
        const usableW = Math.max(10, cw - pad * 2)
        const usableH = Math.max(10, ch - pad * 2)
        const glyph = 0.62
        const lineFactor = 1.2
        const side = Math.min(cw, ch)

        let fsz = Math.max(10, Math.floor(side * 0.20))
        fsz = Math.min(fsz, Math.floor(side * 0.28))

        while (fsz > 8 && Math.floor(usableW / (fsz * glyph)) < maxWord)
            --fsz

        while (fsz > 8) {
            const charsPerLine = Math.max(2, Math.floor(usableW / (fsz * glyph)))
            const maxLines = Math.max(1, Math.floor(usableH / (fsz * lineFactor)))
            if (charsPerLine * maxLines >= Math.ceil(maxLen * 1.1))
                break
            --fsz
        }
        return fsz
    }

    function cellFontSize(label, free) {
        const side = Math.min(cellW, cellH)
        if (free)
            return Math.max(uniformFontSize + 1, Math.min(Math.floor(side * 0.30), uniformFontSize + 6))
        return uniformFontSize
    }

    function maxLabelLines(fsz) {
        const pad = Math.max(2, Math.min(cellW, cellH) * 0.05)
        const usableH = cellH - pad * 2
        const lineH = Math.max(1, fsz * 1.2)
        return Math.max(2, Math.min(12, Math.floor(usableH / lineH)))
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

    GridLayout {
        id: grid
        anchors.fill: parent
        columns: Math.max(1, root.cols)
        rowSpacing: root.gap
        columnSpacing: root.gap
        visible: root.n > 0 && root.cols > 0

        Repeater {
            model: root.n * root.cols
            Rectangle {
                id: cellRect
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: root.fillBounds ? -1 : root.cellW
                Layout.preferredHeight: root.fillBounds ? -1 : root.cellH
                Layout.minimumWidth: 16
                Layout.minimumHeight: 16

                radius: Math.max(2, Math.min(Theme.radius, Math.min(width, height) * 0.12))
                clip: true

                property int rowIndex: Math.floor(index / Math.max(1, root.cols))
                property int colIndex: index % Math.max(1, root.cols)
                property var cell: {
                    if (!root.rows || rowIndex >= root.rows.length)
                        return null
                    const row = root.rows[rowIndex]
                    if (!row || colIndex >= row.length)
                        return null
                    return row[colIndex]
                }
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
                property real cellPad: Math.max(2, Math.min(width, height) * 0.06)
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

                Text {
                    anchors.fill: parent
                    anchors.margins: cellPad
                    anchors.bottomMargin: pointsText.visible
                                          ? cellPad + pointsText.implicitHeight
                                          : cellPad
                    text: isFree ? "FREE" : cellLabel
                    color: isFree ? Theme.accent : Theme.text
                    font.pixelSize: labelSize
                    font.weight: isFree ? Font.Bold : Font.Normal
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.WordWrap
                    elide: Text.ElideNone
                    maximumLineCount: root.maxLabelLines(labelSize)
                    clip: true
                    opacity: checked && !isFree ? 0.92 : 1
                }

                Text {
                    id: pointsText
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: cellPad
                    visible: !root.hidePoints && !isFree && cell
                             && Math.min(cellRect.width, cellRect.height) >= 40
                    text: {
                        if (!cell) return ""
                        if (root.gageMode && cell.gage)
                            return "Gage #" + cell.points
                        if (cell.points !== undefined)
                            return cell.points + " pt"
                        return ""
                    }
                    color: root.gageMode ? Theme.accent : Theme.textDim
                    font.pixelSize: Math.max(6, Math.min(labelSize * 0.7, 12))
                }

                Label {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Math.max(1, Math.min(width, height) * 0.04)
                    visible: checked && !isFree
                    text: "✓"
                    color: inBingo ? Theme.success : Theme.accent
                    font.pixelSize: Math.max(10, Math.min(Math.min(cellRect.width, cellRect.height) * 0.28, 22))
                    font.weight: Font.Bold
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
                        const cw = root.width / Math.max(1, root.cols)
                        const ch = root.height / Math.max(1, root.n)
                        const r = Math.min(root.n - 1, Math.max(0, Math.floor(global.y / ch)))
                        const c = Math.min(root.cols - 1, Math.max(0, Math.floor(global.x / cw)))
                        if (!(r === root.dragFromRow && c === root.dragFromCol)) {
                            const dest = root.rows[r] && root.rows[r][c]
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
