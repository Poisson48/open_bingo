import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    anchors.margins: Theme.pad
    spacing: Theme.gap

    ComboBox {
        id: playerBox
        Layout.fillWidth: true
        model: AppController.grids
        textRole: "player"
    }

    Label {
        text: playerBox.count > 0
            ? "Score : " + AppController.computeScore(
                  AppController.grids[playerBox.currentIndex].player, playRoot.checks) + " pts"
            : "Générez des grilles d'abord."
        color: Theme.text
    }

    Item {
        id: playRoot
        Layout.fillWidth: true
        Layout.fillHeight: true
        property var checks: []

        function reloadChecks() {
            if (playerBox.count <= 0) { checks = []; return }
            checks = AppController.loadPlayChecks(AppController.grids[playerBox.currentIndex].player)
            if (checks.length === 0 && AppController.gridSize > 0) {
                var empty = []
                for (var r = 0; r < AppController.gridSize; r++) {
                    var row = []
                    for (var c = 0; c < AppController.gridSize; c++)
                        row.push(AppController.freeCenter && r === Math.floor(AppController.gridSize/2)
                                 && c === Math.floor(AppController.gridSize/2))
                    empty.push(row)
                }
                checks = empty
            }
        }

        function toggle(r, c) {
            if (!checks[r]) return
            checks[r][c] = !checks[r][c]
            checks = checks.slice(0)
            AppController.savePlayChecks(AppController.grids[playerBox.currentIndex].player, checks)
        }

        Component.onCompleted: reloadChecks()

        Column {
            anchors.fill: parent
            spacing: 4
            visible: playerBox.count > 0
            Repeater {
                model: playerBox.count > 0 ? AppController.grids[playerBox.currentIndex].cells : []
                Row {
                    spacing: 4
                    property int rowIndex: index
                    Repeater {
                        model: modelData
                        Rectangle {
                            width: 72; height: 56
                            radius: 8
                            property int colIndex: index
                            color: modelData.isFree || (playRoot.checks[rowIndex] && playRoot.checks[rowIndex][colIndex])
                                   ? Theme.accentSoft : Theme.surfaceHigh
                            border.color: Theme.outline
                            Label {
                                anchors.centerIn: parent
                                width: parent.width - 4
                                text: modelData.label
                                wrapMode: Text.WordWrap
                                horizontalAlignment: Text.AlignHCenter
                                font.pixelSize: 9
                            }
                            MouseArea {
                                anchors.fill: parent
                                enabled: !modelData.isFree
                                onClicked: playRoot.toggle(rowIndex, colIndex)
                            }
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: playerBox
        function onCurrentIndexChanged() { playRoot.reloadChecks() }
    }
}
