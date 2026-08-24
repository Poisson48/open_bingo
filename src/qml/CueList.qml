import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 6

    property var cues: []
    property var selected: ({}) // index → true
    property bool skipSfx: true
    property int maxLen: 60

    readonly property int selectedCount: {
        let n = 0
        const s = selected || {}
        for (const k in s) {
            if (s[k])
                ++n
        }
        return n
    }

    signal selectionChanged

    function formatTs(ms) {
        const t = Math.max(0, Math.floor(Number(ms) || 0) / 1000)
        const h = Math.floor(t / 3600)
        const m = Math.floor((t % 3600) / 60)
        const s = Math.floor(t % 60)
        const pad = n => (n < 10 ? "0" : "") + n
        return pad(h) + ":" + pad(m) + ":" + pad(s)
    }

    function cueText(c) {
        if (!c)
            return ""
        return c.plain || c.text || ""
    }

    function isSfx(c) {
        if (!c)
            return false
        if (c.likelySfx !== undefined)
            return !!c.likelySfx
        if (c.likely_sfx !== undefined)
            return !!c.likely_sfx
        return false
    }

    function isUseful(index) {
        const c = cues[index]
        if (!c)
            return false
        if (skipSfx && isSfx(c))
            return false
        const t = cueText(c).trim()
        if (t.length === 0)
            return false
        if (maxLen > 0 && t.length > maxLen)
            return false
        return true
    }

    function isChecked(index) {
        return !!(selected && selected[index])
    }

    function setChecked(index, on) {
        const next = Object.assign({}, selected || {})
        if (on)
            next[index] = true
        else
            delete next[index]
        selected = next
        selectionChanged()
    }

    function selectUseful() {
        const next = {}
        for (let i = 0; i < cues.length; ++i) {
            if (isUseful(i))
                next[i] = true
        }
        selected = next
        selectionChanged()
    }

    function syncFromModel() {
        const next = {}
        for (let i = 0; i < cues.length; ++i) {
            const c = cues[i]
            if (c && c.selected)
                next[i] = true
            else if (c && c.selected === undefined && isUseful(i))
                next[i] = true
        }
        selected = next
        selectionChanged()
    }

    function clearSelection() {
        selected = {}
        selectionChanged()
    }

    function selectedIndices() {
        const out = []
        const s = selected || {}
        for (const k in s) {
            if (s[k])
                out.push(Number(k))
        }
        out.sort((a, b) => a - b)
        return out
    }

    onCuesChanged: Qt.callLater(syncFromModel)

    Repeater {
        model: root.cues

        Rectangle {
            Layout.fillWidth: true
            radius: Theme.radius
            color: root.isChecked(index) ? Theme.accentSoft : Theme.inputBg
            border.color: root.isChecked(index) ? Theme.accent : Theme.outlineLight
            implicitHeight: Math.max(Theme.touchTarget, cueRow.implicitHeight + 12)
            opacity: (root.skipSfx && root.isSfx(modelData)) ? 0.45 : 1

            Behavior on opacity { NumberAnimation { duration: 120 } }
            Behavior on color { ColorAnimation { duration: 120 } }

            RowLayout {
                id: cueRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 8
                spacing: 8

                ColoCheckBox {
                    checked: root.isChecked(index)
                    text: ""
                    Layout.preferredWidth: Theme.touchTarget
                    onClicked: root.setChecked(index, checked)
                }

                Label {
                    text: root.formatTs(modelData.startMs !== undefined
                                        ? modelData.startMs : modelData.start_ms)
                    color: Theme.textDim
                    font.pixelSize: 12
                    font.family: "monospace"
                    Layout.preferredWidth: 72
                }

                Label {
                    Layout.fillWidth: true
                    text: root.cueText(modelData)
                    color: Theme.text
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                }
            }

            MouseArea {
                anchors.fill: parent
                anchors.leftMargin: Theme.touchTarget
                onClicked: root.setChecked(index, !root.isChecked(index))
            }
        }
    }
}
