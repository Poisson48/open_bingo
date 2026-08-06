import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: scroll
    clip: true
    contentWidth: availableWidth

    property var board: []
    readonly property var winners: {
        var out = []
        for (var i = 0; i < board.length; ++i)
            if (board[i].full)
                out.push(board[i])
        return out
    }
    readonly property bool gageMode: board.length > 0 && !!board[0].gageMode

    function refresh() {
        board = AppController.playScoreboard()
    }

    function formatScore(entry) {
        const n = Number(entry.score || 0).toLocaleString(Qt.locale("fr_FR"))
        return n + " " + (entry.unit || "pts")
    }

    Component.onCompleted: refresh()
    Connections {
        target: AppController
        function onPlayChecksChanged() { scroll.refresh() }
        function onCurrentProjectChanged() { scroll.refresh() }
    }

    ColumnLayout {
        x: Theme.pad
        width: Math.max(0, scroll.availableWidth - Theme.pad * 2)
        spacing: Theme.gap

        Label {
            Layout.fillWidth: true
            text: "Classement live — grille pleine = gagnant. Exportez en PNG pour partager."
            color: Theme.textDim
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }

        Rectangle {
            Layout.fillWidth: true
            visible: scroll.winners.length > 0
            implicitHeight: winBanner.implicitHeight + 24
            radius: Theme.radiusLg
            color: Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.15)
            border.color: Theme.success
            border.width: 2

            ColumnLayout {
                id: winBanner
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: Theme.pad
                spacing: 4

                Label {
                    Layout.fillWidth: true
                    text: scroll.winners.length === 1 ? "Gagnant" : "Gagnants"
                    color: Theme.success
                    font.pixelSize: 12
                    font.weight: Font.Bold
                }
                Label {
                    Layout.fillWidth: true
                    text: {
                        var names = []
                        for (var i = 0; i < scroll.winners.length; ++i)
                            names.push(scroll.winners[i].player || "")
                        return names.join(" · ")
                    }
                    color: Theme.text
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    wrapMode: Text.WordWrap
                }
                Label {
                    Layout.fillWidth: true
                    visible: scroll.winners.length === 1
                    text: scroll.winners.length === 1
                          ? ("Grille complète — " + scroll.formatScore(scroll.winners[0]))
                          : ""
                    color: Theme.textDim
                    font.pixelSize: 13
                }
            }
        }

        BingoButton {
            Layout.fillWidth: true
            text: "Exporter en PNG"
            primary: true
            enabled: scroll.board.length > 0
            onClicked: AppController.saveScoreboardPng()
        }

        Label {
            visible: scroll.board.length === 0
            Layout.fillWidth: true
            text: "Générez des grilles et cochez des cases dans Play pour voir le classement."
            color: Theme.textDim
            font.pixelSize: 14
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            topPadding: 24
        }

        Repeater {
            model: scroll.board

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: rowInner.implicitHeight + 24
                radius: Theme.radiusLg
                color: modelData.full ? Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.12)
                     : index === 0 ? Theme.surfaceHigh : Theme.surface
                border.color: modelData.full ? Theme.success
                             : index === 0 ? Theme.accent : Theme.outline
                border.width: modelData.full || index === 0 ? 1.5 : 1

                ColumnLayout {
                    id: rowInner
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: Theme.pad
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Rectangle {
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 36
                            radius: 18
                            color: index === 0 ? Theme.warning
                                 : index === 1 ? "#e2e8f0"
                                 : index === 2 ? "#d97706"
                                 : Theme.outlineLight
                            Label {
                                anchors.centerIn: parent
                                text: (index + 1).toString()
                                color: index < 3 ? Theme.background : Theme.text
                                font.pixelSize: 14
                                font.weight: Font.Bold
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Label {
                                Layout.fillWidth: true
                                text: modelData.player || ""
                                color: Theme.text
                                font.pixelSize: index < 3 ? 16 : 15
                                font.weight: index < 3 || modelData.full ? Font.DemiBold : Font.Normal
                                elide: Text.ElideRight
                                maximumLineCount: 2
                                wrapMode: Text.WordWrap
                            }
                            Label {
                                visible: !!modelData.full
                                text: "Gagnant — grille complète"
                                color: Theme.success
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                            }
                            Label {
                                visible: !modelData.full
                                text: (modelData.checked || 0) + " / " + (modelData.total || 0) + " cases"
                                color: Theme.textDim
                                font.pixelSize: 12
                            }
                        }

                        Label {
                            text: scroll.formatScore(modelData)
                            color: modelData.full ? Theme.success : Theme.accent
                            font.pixelSize: 17
                            font.weight: Font.Bold
                            horizontalAlignment: Text.AlignRight
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 8
                        radius: 4
                        color: Theme.inputBg
                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: {
                                const t = Math.max(1, modelData.total || 1)
                                return parent.width * Math.min(1, (modelData.checked || 0) / t)
                            }
                            radius: 4
                            color: modelData.full ? Theme.success : Theme.accent
                        }
                    }
                }
            }
        }

        Item { Layout.preferredHeight: Theme.pad * 2 }
    }
}
