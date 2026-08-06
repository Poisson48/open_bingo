import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: scroll
    clip: true
    contentWidth: availableWidth

    property var board: []

    function refresh() {
        board = AppController.playScoreboard()
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
            text: "Classement live — grille pleine en tête, puis score. Exportez en PNG pour partager."
            color: Theme.textDim
            font.pixelSize: 13
            wrapMode: Text.WordWrap
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
                implicitHeight: rowInner.implicitHeight + 20
                radius: Theme.radiusLg
                color: index === 0 ? Theme.surfaceHigh : Theme.surface
                border.color: index === 0 ? Theme.accent : Theme.outline
                border.width: index === 0 ? 1.5 : 1

                RowLayout {
                    id: rowInner
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: Theme.pad
                    spacing: 12

                    Rectangle {
                        Layout.preferredWidth: 36
                        Layout.preferredHeight: 36
                        radius: 18
                        color: index === 0 ? Theme.warning
                             : index === 1 ? "#94a3b8"
                             : index === 2 ? "#c08457"
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
                            font.weight: index < 3 ? Font.DemiBold : Font.Normal
                            elide: Text.ElideRight
                            maximumLineCount: 2
                            wrapMode: Text.WordWrap
                        }
                        RowLayout {
                            spacing: 8
                            Label {
                                text: (modelData.checked || 0) + " case"
                                      + ((modelData.checked || 0) > 1 ? "s" : "")
                                color: Theme.textDim
                                font.pixelSize: 12
                            }
                            Rectangle {
                                visible: !!modelData.full
                                radius: 6
                                color: Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.2)
                                implicitWidth: fullLbl.implicitWidth + 12
                                implicitHeight: 18
                                Label {
                                    id: fullLbl
                                    anchors.centerIn: parent
                                    text: "FULL"
                                    color: Theme.success
                                    font.pixelSize: 10
                                    font.weight: Font.Bold
                                }
                            }
                        }
                    }

                    Label {
                        text: Number(modelData.score || 0).toLocaleString(Qt.locale("fr_FR"))
                              + " pts"
                        color: modelData.full ? Theme.success : Theme.accent
                        font.pixelSize: 18
                        font.weight: Font.Bold
                        horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignVCenter
                    }
                }
            }
        }

        Item { Layout.preferredHeight: Theme.pad * 2 }
    }
}
