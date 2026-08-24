import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    required property var modelData
    property bool selected: false

    readonly property bool isSdh: {
        const d = modelData || {}
        const hi = d.hearing_impaired !== undefined ? d.hearing_impaired
                 : (d.hearingImpaired !== undefined ? d.hearingImpaired : d.hi)
        if (typeof hi === "boolean")
            return hi
        if (typeof hi === "number")
            return hi !== 0
        const s = String(hi || "").toLowerCase()
        return s === "true" || s === "1" || s === "yes" || s === "hi"
    }

    readonly property string titleText: {
        const d = modelData || {}
        return d.title || d.featureTitle || d.movieName || d.name || "Sans titre"
    }

    readonly property string yearText: {
        const d = modelData || {}
        const y = d.year || d.release_year || ""
        return y ? String(y) : ""
    }

    readonly property string langText: {
        const d = modelData || {}
        const l = d.language || d.lang || ""
        return l ? String(l).toUpperCase() : ""
    }

    readonly property string metaLine: {
        const d = modelData || {}
        const parts = []
        const dl = d.download_count !== undefined ? d.download_count : d.downloadCount
        if (dl !== undefined && dl !== null && dl !== "")
            parts.push(formatCount(dl) + " DL")
        const rel = d.release || d.releaseName || d.release_name || ""
        if (rel)
            parts.push(String(rel))
        const fps = d.fps
        if (fps !== undefined && fps !== null && fps !== "")
            parts.push("fps " + fps)
        return parts.join(" · ")
    }

    signal activated

    radius: Theme.radius
    color: selected ? Theme.accentSoft : Theme.surface
    border.color: selected ? Theme.accent : Theme.outline
    implicitHeight: Math.max(Theme.touchTarget, col.implicitHeight + 20)

    function formatCount(n) {
        const v = Number(n)
        if (isNaN(v))
            return String(n)
        if (v >= 1000)
            return (v / 1000).toFixed(v >= 10000 ? 0 : 1).replace(/\.0$/, "") + "k"
        return String(Math.round(v))
    }

    ColumnLayout {
        id: col
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.margins: 10
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: {
                    let t = root.titleText
                    if (root.yearText)
                        t += " (" + root.yearText + ")"
                    if (root.langText)
                        t += " · " + root.langText
                    return t
                }
                color: Theme.text
                font.pixelSize: 14
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                maximumLineCount: 2
                wrapMode: Text.WordWrap
            }

            Rectangle {
                visible: root.isSdh
                radius: 4
                color: Theme.accentSoft
                implicitWidth: sdhLabel.implicitWidth + 12
                implicitHeight: Math.max(22, sdhLabel.implicitHeight + 4)
                Label {
                    id: sdhLabel
                    anchors.centerIn: parent
                    text: "SDH"
                    color: Theme.accent
                    font.pixelSize: 11
                    font.weight: Font.Bold
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: root.metaLine.length > 0
            text: root.metaLine
            color: Theme.textDim
            font.pixelSize: 12
            elide: Text.ElideRight
            maximumLineCount: 1
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.activated()
    }

    Behavior on color { ColorAnimation { duration: 140 } }
    Behavior on border.color { ColorAnimation { duration: 140 } }
}
