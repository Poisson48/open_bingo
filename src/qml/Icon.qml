import QtQuick

// Icônes vectorielles — jamais de glyphes Unicode (tofu Android).
// Noms : menu | grip | close | back | check | search | plus | trash |
//        up | down | play | share | print | warn | chevron
Item {
    id: icon
    property string name: "menu"
    property color color: "#FFFFFF"
    property real size: 20
    property real thickness: Math.max(1.5, size / 11)

    implicitWidth: size
    implicitHeight: size

    onColorChanged: strokes.requestPaint()
    onNameChanged: strokes.requestPaint()
    onSizeChanged: strokes.requestPaint()
    Component.onCompleted: strokes.requestPaint()

    Grid {
        anchors.centerIn: parent
        visible: icon.name === "menu" || icon.name === "grip"
        readonly property real dot: Math.max(2, icon.size / 7)
        columns: icon.name === "grip" ? 2 : 1
        rows: 3
        spacing: icon.size / 7
        Repeater {
            model: icon.name === "grip" ? 6 : 3
            Rectangle {
                width: parent.dot
                height: parent.dot
                radius: parent.dot / 2
                color: icon.color
            }
        }
    }

    Canvas {
        id: strokes
        anchors.centerIn: parent
        width: icon.size
        height: icon.size
        visible: !(icon.name === "menu" || icon.name === "grip")

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = icon.color
            ctx.fillStyle = icon.color
            ctx.lineWidth = icon.thickness
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            const w = width
            const h = height
            ctx.beginPath()

            if (icon.name === "close") {
                const m = w * 0.28
                ctx.moveTo(m, m); ctx.lineTo(w - m, h - m)
                ctx.moveTo(w - m, m); ctx.lineTo(m, h - m)
                ctx.stroke()
            } else if (icon.name === "back") {
                const x = w * 0.34
                ctx.moveTo(x + w * 0.16, h * 0.24)
                ctx.lineTo(x - w * 0.06, h * 0.5)
                ctx.lineTo(x + w * 0.16, h * 0.76)
                ctx.moveTo(x - w * 0.04, h * 0.5)
                ctx.lineTo(w * 0.80, h * 0.5)
                ctx.stroke()
            } else if (icon.name === "check") {
                ctx.moveTo(w * 0.22, h * 0.52)
                ctx.lineTo(w * 0.42, h * 0.72)
                ctx.lineTo(w * 0.78, h * 0.28)
                ctx.stroke()
            } else if (icon.name === "search") {
                const r = w * 0.26
                const cx = w * 0.44
                const cy = h * 0.44
                ctx.arc(cx, cy, r, 0, Math.PI * 2)
                ctx.moveTo(cx + r * 0.72, cy + r * 0.72)
                ctx.lineTo(w * 0.82, h * 0.82)
                ctx.stroke()
            } else if (icon.name === "plus") {
                ctx.moveTo(w * 0.5, h * 0.22); ctx.lineTo(w * 0.5, h * 0.78)
                ctx.moveTo(w * 0.22, h * 0.5); ctx.lineTo(w * 0.78, h * 0.5)
                ctx.stroke()
            } else if (icon.name === "trash") {
                ctx.moveTo(w * 0.28, h * 0.34); ctx.lineTo(w * 0.72, h * 0.34)
                ctx.moveTo(w * 0.36, h * 0.34); ctx.lineTo(w * 0.40, h * 0.22)
                ctx.lineTo(w * 0.60, h * 0.22); ctx.lineTo(w * 0.64, h * 0.34)
                ctx.moveTo(w * 0.34, h * 0.34); ctx.lineTo(w * 0.38, h * 0.80)
                ctx.lineTo(w * 0.62, h * 0.80); ctx.lineTo(w * 0.66, h * 0.34)
                ctx.moveTo(w * 0.46, h * 0.44); ctx.lineTo(w * 0.46, h * 0.70)
                ctx.moveTo(w * 0.54, h * 0.44); ctx.lineTo(w * 0.54, h * 0.70)
                ctx.stroke()
            } else if (icon.name === "up") {
                ctx.moveTo(w * 0.28, h * 0.58)
                ctx.lineTo(w * 0.50, h * 0.32)
                ctx.lineTo(w * 0.72, h * 0.58)
                ctx.stroke()
            } else if (icon.name === "down") {
                ctx.moveTo(w * 0.28, h * 0.42)
                ctx.lineTo(w * 0.50, h * 0.68)
                ctx.lineTo(w * 0.72, h * 0.42)
                ctx.stroke()
            } else if (icon.name === "play") {
                ctx.moveTo(w * 0.32, h * 0.22)
                ctx.lineTo(w * 0.78, h * 0.50)
                ctx.lineTo(w * 0.32, h * 0.78)
                ctx.closePath()
                ctx.fill()
            } else if (icon.name === "share") {
                ctx.arc(w * 0.28, h * 0.50, w * 0.10, 0, Math.PI * 2)
                ctx.arc(w * 0.72, h * 0.28, w * 0.10, 0, Math.PI * 2)
                ctx.arc(w * 0.72, h * 0.72, w * 0.10, 0, Math.PI * 2)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(w * 0.36, h * 0.46); ctx.lineTo(w * 0.64, h * 0.32)
                ctx.moveTo(w * 0.36, h * 0.54); ctx.lineTo(w * 0.64, h * 0.68)
                ctx.stroke()
            } else if (icon.name === "print") {
                ctx.rect(w * 0.28, h * 0.18, w * 0.44, h * 0.22)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(w * 0.22, h * 0.40); ctx.lineTo(w * 0.78, h * 0.40)
                ctx.lineTo(w * 0.78, h * 0.68); ctx.lineTo(w * 0.22, h * 0.68)
                ctx.closePath()
                ctx.stroke()
                ctx.beginPath()
                ctx.rect(w * 0.34, h * 0.62, w * 0.32, h * 0.20)
                ctx.stroke()
            } else if (icon.name === "warn") {
                ctx.moveTo(w * 0.50, h * 0.18)
                ctx.lineTo(w * 0.82, h * 0.78)
                ctx.lineTo(w * 0.18, h * 0.78)
                ctx.closePath()
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(w * 0.50, h * 0.38); ctx.lineTo(w * 0.50, h * 0.56)
                ctx.stroke()
                ctx.beginPath()
                ctx.arc(w * 0.50, h * 0.66, w * 0.04, 0, Math.PI * 2)
                ctx.fill()
            } else if (icon.name === "chevron") {
                ctx.moveTo(w * 0.38, h * 0.28)
                ctx.lineTo(w * 0.62, h * 0.50)
                ctx.lineTo(w * 0.38, h * 0.72)
                ctx.stroke()
            } else {
                // Fallback: petit point
                ctx.arc(w * 0.5, h * 0.5, w * 0.12, 0, Math.PI * 2)
                ctx.fill()
            }
        }
    }
}
