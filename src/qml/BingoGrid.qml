import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Grille bingo responsive : texte contenu dans chaque cellule, pas de débordement.
Column {
    id: root
    property var rows: []
    property real availableWidth: 320
    property real availableHeight: 0
    property bool interactive: false
    property var checks: null
    property var bingoSet: ({})
    property bool gageMode: false
    property var gages: []
    signal cellClicked(int row, int col)

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

    function cellFontSize(label, free) {
        if (free)
            return Math.min(9, Math.max(7, cellH * 0.18))
        const len = label ? label.length : 0
        let base = Math.max(7, Math.min(cellW, cellH) * 0.22)
        if (len > 50)
            base *= 0.65
        else if (len > 30)
            base *= 0.75
        else if (len > 18)
            base *= 0.85
        return Math.min(base, 13)
    }

    function maxLabelLines(fsz) {
        const lineH = fsz * 1.2
        const budget = cellH - (cellH > 36 ? fsz * 0.85 : 0) - 8
        return Math.max(1, Math.floor(budget / lineH))
    }

    Repeater {
        model: root.n
        Row {
            spacing: root.gap
            property int rowIndex: index
            Repeater {
                model: root.cols
                Rectangle {
                    width: root.cellW
                    height: root.cellH
                    radius: Theme.radius
                    clip: true
                    property int colIndex: index
                    property var cell: root.rows[rowIndex][colIndex]
                    property bool isFree: cell && cell.isFree
                    property bool checked: root.checks && root.checks[rowIndex]
                                         && root.checks[rowIndex][colIndex]
                    property bool inBingo: root.bingoSet[rowIndex + "," + colIndex]
                    property bool marked: isFree || checked
                    property string cellLabel: cell && cell.label ? cell.label : ""
                    property real labelSize: root.cellFontSize(cellLabel, isFree)

                    color: inBingo ? Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.35)
                         : marked ? Theme.accentSoft : Theme.inputBg
                    border.color: inBingo ? Theme.success
                                : marked ? Theme.accent : Theme.outline
                    border.width: inBingo ? 2 : 1

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
                        enabled: root.interactive && !isFree
                        onClicked: root.cellClicked(rowIndex, colIndex)
                    }
                }
            }
        }
    }
}
