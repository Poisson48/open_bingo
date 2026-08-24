import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

// Bingo film — OpenSubtitles → cues → phrases. Desktop StackView (Main.pushFilmAssist).
Item {
    id: root
    property string pageTitle: "Bingo film"
    property bool isFilmAssist: true

    property Component actions: Item {}

    readonly property var film: AppController.film
    readonly property bool desktop: Qt.platform.os !== "android"
    readonly property bool hasKey: film && film.hasApiKey
    readonly property bool canSearch: film && film.canSearch
    readonly property string osSource: film ? (film.source || "auto") : "auto"
    readonly property bool busy: film && film.busy
    readonly property string errorText: film ? (film.error || "") : ""
    readonly property var results: film ? film.results : []
    readonly property var cues: film ? film.cues : []

    property bool didSearch: false
    property int selectedFileId: -1
    property bool skipSfx: true
    property int maxLen: 60

    readonly property var sourceModel: [
        { code: "auto", label: "Auto" },
        { code: "com", label: ".com" },
        { code: "org", label: ".org" }
    ]

    readonly property var langModel: [
        { code: "fr", label: "Français" },
        { code: "en", label: "English" },
        { code: "es", label: "Español" },
        { code: "de", label: "Deutsch" },
        { code: "it", label: "Italiano" },
        { code: "pt", label: "Português" },
        { code: "nl", label: "Nederlands" },
        { code: "pl", label: "Polski" },
        { code: "ru", label: "Русский" },
        { code: "sv", label: "Svenska" },
        { code: "ja", label: "日本語" },
        { code: "ko", label: "한국어" },
        { code: "zh", label: "中文" },
        { code: "ar", label: "العربية" },
        { code: "tr", label: "Türkçe" },
        { code: "cs", label: "Čeština" },
        { code: "hu", label: "Magyar" },
        { code: "ro", label: "Română" },
        { code: "uk", label: "Українська" },
        { code: "he", label: "עברית" }
    ]

    function handleBack() { return false }

    function fileIdOf(item) {
        if (!item)
            return -1
        const id = item.file_id !== undefined ? item.file_id
                 : (item.fileId !== undefined ? item.fileId : item.id)
        const n = Number(id)
        return isNaN(n) ? -1 : n
    }

    function syncLangCombo() {
        if (!film)
            return
        const code = (film.language || "").toLowerCase()
        let idx = -1
        for (let i = 0; i < langModel.length; ++i) {
            if (langModel[i].code === code) {
                idx = i
                break
            }
        }
        if (idx >= 0)
            langCombo.currentIndex = idx
        else if (!(film.language && film.language.length)) {
            langCombo.currentIndex = 0
            film.language = langModel[0].code
        }
        // Langue hors liste : on ne touche pas au combo (évite d’écraser le settings).
    }

    function doSearch() {
        if (!film || !canSearch || busy)
            return
        film.clearError()
        film.query = searchBar.text.trim()
        if (film.query.length === 0)
            return
        didSearch = true
        selectedFileId = -1
        cueList.clearSelection()
        film.search()
    }

    function syncSourceCombo() {
        if (!film)
            return
        const code = (film.source || "auto").toLowerCase()
        let idx = 0
        for (let i = 0; i < sourceModel.length; ++i) {
            if (sourceModel[i].code === code) {
                idx = i
                break
            }
        }
        sourceCombo.currentIndex = idx
    }

    function pickResult(item) {
        if (!film || busy)
            return
        const id = fileIdOf(item)
        if (id < 0)
            return
        selectedFileId = id
        cueList.clearSelection()
        film.clearError()
        film.downloadAndPreview(id)
    }

    function importPhrases() {
        if (!film || busy)
            return
        // Sélection UI locale ; C++ actuel = filtres skipSfx/maxLen seulement.
        const r = film.importSelectedCues(skipSfx, maxLen, cueList.selectedIndices())
        if (r && r.ok) {
            const n = r.added !== undefined ? r.added : 0
            const w = root.Window.window
            if (w && typeof w.showToast === "function")
                w.showToast(n + " phrase(s) ajoutée(s)")
        } else if (r && r.errors && r.errors.length) {
            const w = root.Window.window
            if (w && typeof w.showToast === "function")
                w.showToast(String(r.errors[0]))
        }
    }

    Component.onCompleted: {
        if (film) {
            searchBar.text = film.query || ""
            syncLangCombo()
            syncSourceCombo()
        }
    }

    Connections {
        target: film
        function onLanguageChanged() { syncLangCombo() }
        function onSourceChanged() { syncSourceCombo() }
        function onCuesChanged() {
            if (cues && cues.length > 0)
                Qt.callLater(function () { cueList.syncFromModel() })
        }
        function onResultsChanged() {
            selectedFileId = -1
        }
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
                text: "Sous-titres malentendants → phrases bingo, sans spoiler visuel."
                color: Theme.textDim
                wrapMode: Text.WordWrap
                font.pixelSize: 13
            }

            // —— Android / no desktop ——
            Label {
                Layout.fillWidth: true
                visible: !root.desktop
                text: "Recherche OpenSubtitles disponible sur PC uniquement."
                color: Theme.warning
                wrapMode: Text.WordWrap
            }

            // —— No API key (source .com only) ——
            Rectangle {
                Layout.fillWidth: true
                visible: root.desktop && !root.canSearch
                radius: Theme.radiusLg
                color: Theme.surface
                border.color: Theme.warning
                implicitHeight: noKeyCol.implicitHeight + 24
                opacity: visible ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 160 } }

                ColumnLayout {
                    id: noKeyCol
                    anchors.fill: parent
                    anchors.margins: Theme.pad
                    spacing: 8

                    Label {
                        Layout.fillWidth: true
                        text: "Ajoute ta clé OpenSubtitles (gratuite) dans Réglages pour chercher des films."
                        color: Theme.text
                        wrapMode: Text.WordWrap
                        font.pixelSize: 14
                    }
                    Label {
                        Layout.fillWidth: true
                        text: "Réglages → carte OpenSubtitles (clé API), ou passe la source en Auto / .org."
                        color: Theme.textDim
                        wrapMode: Text.WordWrap
                        font.pixelSize: 12
                    }
                }
            }

            // —— Search (desktop + canSearch) ——
            ColumnLayout {
                Layout.fillWidth: true
                visible: root.desktop && root.canSearch
                spacing: Theme.gap
                opacity: visible ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 160 } }

                FilmSearchBar {
                    id: searchBar
                    Layout.fillWidth: true
                    busy: root.busy
                    searchEnabled: root.canSearch
                    onSearchRequested: root.doSearch()
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.gap

                    Label {
                        text: "Langue"
                        color: Theme.textDim
                        font.pixelSize: 13
                    }
                    ColoComboBox {
                        id: langCombo
                        Layout.fillWidth: true
                        model: root.langModel
                        textRole: "label"
                        enabled: !root.busy
                        onActivated: {
                            if (film && currentIndex >= 0)
                                film.language = root.langModel[currentIndex].code
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.gap

                    Label {
                        text: "Source"
                        color: Theme.textDim
                        font.pixelSize: 13
                    }
                    ColoComboBox {
                        id: sourceCombo
                        Layout.fillWidth: true
                        model: root.sourceModel
                        textRole: "label"
                        enabled: !root.busy
                        onActivated: {
                            if (film && currentIndex >= 0)
                                film.source = root.sourceModel[currentIndex].code
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: "SDH mis en avant (include) — badge « SDH » sur les pistes malentendants."
                    color: Theme.textDim
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }

            // —— Error ——
            Rectangle {
                Layout.fillWidth: true
                visible: root.desktop && root.errorText.length > 0
                radius: Theme.radius
                color: Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.12)
                border.color: Theme.danger
                implicitHeight: errCol.implicitHeight + 20
                opacity: visible ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 160 } }

                ColumnLayout {
                    id: errCol
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 6

                    Label {
                        Layout.fillWidth: true
                        text: {
                            const e = root.errorText.toLowerCase()
                            if (e.indexOf("quota") >= 0 || e.indexOf("429") >= 0
                                    || e.indexOf("download limit") >= 0)
                                return "Plus de téléchargements aujourd’hui (OpenSubtitles). Réessaie demain ou connecte un compte."
                            return root.errorText
                        }
                        color: Theme.danger
                        wrapMode: Text.WordWrap
                        font.pixelSize: 13
                    }
                    BingoButton {
                        text: "Effacer"
                        onClicked: { if (film) film.clearError() }
                    }
                }
            }

            // —— Loading ——
            RowLayout {
                Layout.fillWidth: true
                visible: root.desktop && root.busy
                spacing: 10
                opacity: visible ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 140 } }

                BusyIndicator {
                    running: root.busy
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                }
                Label {
                    Layout.fillWidth: true
                    text: root.cues.length === 0 && root.selectedFileId >= 0
                          ? "Téléchargement des sous-titres…"
                          : "Recherche en cours…"
                    color: Theme.textDim
                    font.pixelSize: 13
                }
            }

            // —— Results ——
            Label {
                Layout.fillWidth: true
                visible: root.desktop && root.hasKey
                         && !root.busy && root.didSearch && root.results.length > 0
                text: "Résultats"
                color: Theme.text
                font.weight: Font.DemiBold
                font.pixelSize: 14
            }

            Label {
                Layout.fillWidth: true
                visible: root.desktop && root.hasKey && root.didSearch
                         && !root.busy && root.errorText.length === 0
                         && root.results.length === 0
                text: "Aucun sous-titre trouvé. Essaie un autre titre, une année, ou change de langue."
                color: Theme.textDim
                wrapMode: Text.WordWrap
                font.pixelSize: 13
                opacity: visible ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 160 } }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                visible: root.desktop && root.hasKey && root.results.length > 0
                opacity: visible ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 180 } }

                Repeater {
                    model: root.results
                    SubtitleResultDelegate {
                        Layout.fillWidth: true
                        modelData: modelData
                        selected: root.fileIdOf(modelData) === root.selectedFileId
                                  && root.selectedFileId >= 0
                        onActivated: root.pickResult(modelData)
                    }
                }
            }

            // —— Preview cues ——
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.gap
                visible: root.desktop && root.hasKey && root.cues.length > 0
                opacity: visible ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 180 } }

                Label {
                    Layout.fillWidth: true
                    text: "Aperçu"
                    color: Theme.text
                    font.weight: Font.DemiBold
                    font.pixelSize: 14
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 12

                    ColoCheckBox {
                        text: "Ignore bruitages"
                        checked: root.skipSfx
                        onClicked: {
                            root.skipSfx = checked
                            cueList.skipSfx = checked
                            cueList.selectUseful()
                        }
                    }
                    ColoCheckBox {
                        text: "Phrases ≤ 60 car."
                        checked: root.maxLen === 60
                        onClicked: {
                            root.maxLen = checked ? 60 : 0
                            cueList.maxLen = root.maxLen
                            cueList.selectUseful()
                        }
                    }
                }

                CueList {
                    id: cueList
                    Layout.fillWidth: true
                    cues: root.cues
                    skipSfx: root.skipSfx
                    maxLen: root.maxLen
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.gap

                    BingoButton {
                        Layout.fillWidth: true
                        text: "Tout cocher utiles"
                        onClicked: cueList.selectUseful()
                    }
                    BingoButton {
                        Layout.fillWidth: true
                        text: "Ajouter " + cueList.selectedCount + " phrase"
                              + (cueList.selectedCount === 1 ? "" : "s")
                        primary: true
                        enabled: !root.busy && cueList.selectedCount > 0
                        onClicked: root.importPhrases()
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                visible: root.desktop
                text: '<a href="https://www.opensubtitles.com">Sous-titres via OpenSubtitles.com</a>'
                color: Theme.textDim
                font.pixelSize: 11
                textFormat: Text.RichText
                onLinkActivated: function (link) { Qt.openUrlExternally(link) }
                opacity: 0.85
            }

            Item { Layout.preferredHeight: Theme.pad * 2 }
        }
    }
}
