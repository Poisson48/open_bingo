import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Grille bingo responsive.
// - interactive : cocher (Play)
// - editable : glisser pour échanger / cliquer pour remplacer (Grilles)
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
    // Incrémenté par le parent quand les checks changent — force le refresh des cellules.
    property int bingoRevision: 0
    property bool gageMode: false
    property var gages: []

    signal cellClicked(int row, int col)
    signal cellEditRequested(int row, int col, string label)

    spacing: 3
    width: availableWidth
    clip: true

    readonly property int n: rows && rows.length ? rows.length : 0
    readonly property int cols: {
        if (n <= 0 || !rows[0])
            return 0
        const row0 = rows[0]
        return row0.length !== undefined ? row0.length : 0
    }
    readonly property real gap: 3
    readonly property real cellW: cols > 0
        ? Math.max(24, Math.floor((availableWidth - gap * (cols - 1)) / cols))
        : 48
    readonly property real cellH: {
        if (availableHeight <= 0 || n <= 0)
            return Math.max(28, cellW * 0.82)
        const borders = gap * (n - 1)
        return Math.max(24, Math.floor((availableHeight - borders) / n))
    }

    property int dragFromRow: -1
    property int dragFromCol: -1
    property int dragOverRow: -1
    property int dragOverCol: -1
    property bool didDrag: false
    readonly property real dragThreshold: 10

    function cellFontSize(label, free) {
        if (free)
            return Math.min(10, Math.max(8, cellH * 0.2))
        const len = label ? label.length : 0
        let base = Math.max(8, Math.min(cellW, cellH) * 0.26)
        if (len > 40)
            base *= 0.72
        else if (len > 28)
            base *= 0.82
        else if (len > 18)
            base *= 0.9
        return Math.min(base, 14)
    }

    function maxLabelLines(fsz) {
        const lineH = fsz * 1.15
        const ptsReserve = cellH > 40 ? fsz * 0.9 : fsz * 0.75
        const budget = cellH - ptsReserve - 6
        return Math.max(2, Math.min(4, Math.floor(budget / lineH)))
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
                    radius: Theme.radius
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
                    // Important : `=== true` — un binding qui renvoie `undefined`
                    // ne met PAS à jour le bool (le vert resterait collé après décochage).
                    property bool inBingo: {
                        void root.bingoRevision
                        return root.bingoSet[rowIndex + "," + colIndex] === true
                    }
                    property bool marked: isFree || checked
                    property string cellLabel: cell && cell.label ? cell.label : ""
                    property real labelSize: root.cellFontSize(cellLabel, isFree)
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
                        anchors.margins: 3
                        spacing: 1

                        Text {
                            width: parent.width
                            height: parent.height - (pointsText.visible ? pointsText.implicitHeight + 1 : 0)
                            text: isFree ? "FREE" : cellLabel
                            color: isFree ? Theme.accent : Theme.text
                            font.pixelSize: labelSize
                            font.weight: isFree ? Font.Bold : Font.Normal
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            wrapMode: Text.Wrap
                            elide: Text.ElideRight
                            maximumLineCount: root.maxLabelLines(labelSize)
                            clip: true
                        }
                        Text {
                            id: pointsText
                            width: parent.width
                            visible: !isFree && cell
                            text: {
                                if (root.gageMode && cell.gage)
                                    return "Gage #" + cell.points
                                if (cell.points !== undefined)
                                    return cell.points + " pt"
                                return ""
                            }
                            color: root.gageMode ? Theme.accent : Theme.textDim
                            font.pixelSize: Math.max(6, labelSize * 0.72)
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideRight
                            maximumLineCount: 1
                            clip: true
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: checked && !isFree
                        text: "✓"
                        color: inBingo ? Theme.success : Theme.accent
                        font.pixelSize: Math.min(root.cellH * 0.45, 28)
                        font.weight: Font.Bold
                        opacity: 0.9
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
