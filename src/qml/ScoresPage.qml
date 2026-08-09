import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

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

    function openPreview() {
        const url = AppController.prepareScoreboardPreview()
        if (!url || url.length === 0)
            return
        previewPopup.previewUrl = url
        previewPopup.open()
    }

    Component.onCompleted: refresh()
    Connections {
        target: AppController
        function onPlayChecksChanged() { root.refresh() }
        function onCurrentProjectChanged() { root.refresh() }
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            x: Theme.pad
            width: Math.min(Math.max(0, scroll.availableWidth - Theme.pad * 2), Theme.contentMax)
            spacing: Theme.gap

            Label {
                Layout.fillWidth: true
                text: "Classement live — grille pleine = gagnant. Aperçu PNG puis partage / enregistrement."
                color: Theme.textDim
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.fillWidth: true
                visible: root.winners.length > 0
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
                        text: root.winners.length === 1 ? "Gagnant" : "Gagnants"
                        color: Theme.success
                        font.pixelSize: 12
                        font.weight: Font.Bold
                    }
                    Label {
                        Layout.fillWidth: true
                        text: {
                            var names = []
                            for (var i = 0; i < root.winners.length; ++i)
                                names.push(root.winners[i].player || "")
                            return names.join(" · ")
                        }
                        color: Theme.text
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                        wrapMode: Text.WordWrap
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: root.winners.length === 1
                        text: root.winners.length === 1
                              ? ("Grille complète — " + root.formatScore(root.winners[0]))
                              : ""
                        color: Theme.textDim
                        font.pixelSize: 13
                    }
                }
            }

            BingoButton {
                Layout.fillWidth: true
                text: "Aperçu PNG"
                primary: true
                enabled: root.board.length > 0
                onClicked: root.openPreview()
            }

            Label {
                visible: root.board.length === 0
                Layout.fillWidth: true
                text: "Générez des grilles et cochez des cases dans Partie pour voir le classement."
                color: Theme.textDim
                font.pixelSize: 14
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                topPadding: 24
            }

            Repeater {
                model: root.board

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
                                text: root.formatScore(modelData)
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

    Popup {
        id: previewPopup
        property string previewUrl: ""

        parent: Overlay.overlay
        width: Math.min(Overlay.overlay.width - 32, 520)
        height: Math.min(Overlay.overlay.height - 48, 720)
        x: (Overlay.overlay.width - width) / 2
        y: (Overlay.overlay.height - height) / 2
        modal: true
        focus: true
        padding: 16
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.6) }

        background: Rectangle {
            color: Theme.surface
            radius: Theme.radiusLg
            border.color: Theme.outline
            border.width: 1
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.gap

            Label {
                Layout.fillWidth: true
                text: "Aperçu du classement"
                color: Theme.text
                font.pixelSize: 18
                font.weight: Font.DemiBold
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth

                Image {
                    width: Math.max(1, previewPopup.width - 32)
                    source: previewPopup.previewUrl
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    cache: false
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.gap

                BingoButton {
                    Layout.fillWidth: true
                    text: "Fermer"
                    onClicked: previewPopup.close()
                }
                BingoButton {
                    Layout.fillWidth: true
                    text: AppController.scoreboardShareLabel
                    primary: true
                    onClicked: {
                        if (AppController.shareScoreboardPng())
                            previewPopup.close()
                    }
                }
            }
        }
    }
}
