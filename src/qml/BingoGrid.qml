import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Grille bingo responsive : s'adapte à la largeur ET à la hauteur disponibles.
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

    readonly property int n: rows.length > 0 ? rows[0].length : 0
    readonly property real gap: 3
    readonly property real cellW: n > 0
        ? Math.max(24, Math.floor((availableWidth - gap * (n - 1)) / n))
        : 48
    readonly property real cellH: {
        if (availableHeight <= 0 || n <= 0)
            return Math.max(28, cellW * 0.82)
        const borders = gap * (n - 1)
        return Math.max(24, Math.floor((availableHeight - borders) / n))
    }
    readonly property real fontSize: Math.max(8, Math.min(Math.min(cellW, cellH) * 0.22, 14))

    Repeater {
        model: root.rows
        Row {
            spacing: root.gap
            property int rowIndex: index
            Repeater {
                model: modelData
                Rectangle {
                    width: root.cellW
                    height: root.cellH
                    radius: Theme.radius
                    property int colIndex: index
                    property bool isFree: modelData.isFree
                    property bool checked: root.checks && root.checks[rowIndex]
                                         && root.checks[rowIndex][colIndex]
                    property bool inBingo: root.bingoSet[root.rowIndex + "," + colIndex]
                    property bool marked: isFree || checked

                    color: inBingo ? Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.35)
                         : marked ? Theme.accentSoft : Theme.inputBg
                    border.color: inBingo ? Theme.success
                                : marked ? Theme.accent : Theme.outline
                    border.width: inBingo ? 2 : 1

                    Column {
                        anchors.fill: parent
                        anchors.margins: 3
                        spacing: 1

                        Label {
                            width: parent.width
                            text: isFree ? "FREE" : modelData.label
                            color: isFree ? Theme.accent : Theme.text
                            font.pixelSize: isFree ? Math.min(9, root.fontSize) : root.fontSize
                            font.weight: isFree ? Font.Bold : Font.Normal
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                            maximumLineCount: Math.max(1, Math.floor((root.cellH - 10) / (root.fontSize * 1.25)))
                            elide: Text.ElideRight
                        }
                        Label {
                            width: parent.width
                            visible: !isFree
                            text: {
                                if (root.gageMode && modelData.gage)
                                    return "Gage #" + modelData.points
                                if (modelData.points !== undefined)
                                    return modelData.points + " pt"
                                return ""
                            }
                            color: root.gageMode ? Theme.accent : Theme.textDim
                            font.pixelSize: Math.max(7, root.fontSize * 0.75)
                            horizontalAlignment: Text.AlignRight
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
