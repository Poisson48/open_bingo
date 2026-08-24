.pragma library

function findFlickable(item) {
    var p = item ? item.parent : null
    while (p) {
        // Flickable (ScrollView contentItem in Qt Quick Controls 2)
        if (p.contentY !== undefined && p.contentHeight !== undefined
                && p.height !== undefined && typeof p.flickableDirection !== "undefined")
            return p
        if (p.contentItem && p.contentItem.contentY !== undefined
                && typeof p.contentItem.flickableDirection !== "undefined")
            return p.contentItem
        p = p.parent
    }
    return null
}

function viewportHeight(flick, item) {
    var h = flick.height
    if (!Qt.inputMethod || !Qt.inputMethod.visible)
        return h
    try {
        var win = item.Window ? item.Window.window : null
        if (!win)
            return h
        var kb = Qt.inputMethod.keyboardRectangle
        if (!kb || kb.height <= 0)
            return h
        // Bas du flickable en coords fenêtre
        var bottomLeft = flick.mapToItem(win.contentItem, 0, flick.height)
        var overlap = bottomLeft.y - kb.y
        if (overlap > 0)
            h = Math.max(80, h - Math.min(overlap, kb.height))
    } catch (e) {
        // Window/import may be unavailable in some contexts
    }
    return h
}

/*! Fait défiler le ScrollView/Flickable parent pour montrer `item`. */
function ensureVisible(item, margin) {
    if (!item || !item.visible)
        return
    if (margin === undefined || margin === null)
        margin = 20

    var flick = findFlickable(item)
    if (!flick || flick.height <= 0)
        return

    var contentItem = flick.contentItem
    if (!contentItem)
        return

    var pos = item.mapToItem(contentItem, 0, 0)
    var top = pos.y
    var bottom = pos.y + item.height
    var viewH = viewportHeight(flick, item)
    var y = flick.contentY
    var maxY = Math.max(0, flick.contentHeight - viewH)

    if (top - margin < y)
        flick.contentY = Math.max(0, top - margin)
    else if (bottom + margin > y + viewH)
        flick.contentY = Math.min(maxY, bottom + margin - viewH)
}
