import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "scrollutils.js" as ScrollUtils

ScrollView {
    clip: true
    contentWidth: availableWidth

    ColumnLayout {
        x: Theme.pad
        width: Math.min(Math.max(0, availableWidth - Theme.pad * 2), Theme.contentMax)
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: Theme.gap

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: section.implicitHeight + 24
            radius: Theme.radiusLg
            color: Theme.surface
            border.color: Theme.outline

            ColumnLayout {
                id: section
                anchors.fill: parent
                anchors.margins: Theme.pad
                spacing: Theme.gap

                Label { text: "Projet"; color: Theme.text; font.weight: Font.DemiBold; font.pixelSize: 14 }
                ColoTextField {
                    Layout.fillWidth: true
                    text: AppController.title
                    hint: "Titre"
                    onEditingFinished: AppController.title = text
                }
                // TextArea : une ligne unique tronquait le début de la description.
                TextArea {
                    id: descField
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(72, contentHeight + topPadding + bottomPadding)
                    text: AppController.description
                    placeholderText: activeFocus || length > 0 ? "" : "Description (optionnel)"
                    color: Theme.text
                    placeholderTextColor: Theme.textDim
                    font.pixelSize: 14
                    wrapMode: TextEdit.Wrap
                    selectByMouse: true
                    leftPadding: 12
                    rightPadding: 12
                    topPadding: 10
                    bottomPadding: 10
                    background: Rectangle {
                        radius: Theme.radius
                        color: Theme.inputBg
                        border.width: descField.activeFocus ? 2 : 1
                        border.color: descField.activeFocus ? Theme.accent : Theme.outlineLight
                    }
                    onActiveFocusChanged: {
                        if (activeFocus)
                            Qt.callLater(function () { ScrollUtils.ensureVisible(descField) })
                        else
                            AppController.description = text
                    }
                    onCursorRectangleChanged: {
                        if (activeFocus)
                            Qt.callLater(function () { ScrollUtils.ensureVisible(descField) })
                    }
                }

                Label { text: "Grille"; color: Theme.text; font.weight: Font.DemiBold; font.pixelSize: 14 }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "Lignes"; color: Theme.textDim; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    ColoSpinBox {
                        from: 2; to: 12
                        value: AppController.gridRows
                        onValueModified: AppController.gridRows = value
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "Colonnes"; color: Theme.textDim; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    ColoSpinBox {
                        from: 2; to: 12
                        value: AppController.gridCols
                        onValueModified: AppController.gridCols = value
                    }
                }
                Label {
                    Layout.fillWidth: true
                    visible: AppController.gridRows !== AppController.gridCols
                    text: "Grille " + AppController.gridRows + "×" + AppController.gridCols
                          + " — les diagonales (bingo / gages) ne comptent que sur une grille carrée."
                    color: Theme.textDim
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }

                ColoCheckBox {
                    Layout.fillWidth: true
                    text: "Case libre au centre"
                    checked: AppController.freeCenter
                    onClicked: AppController.freeCenter = checked
                }
                ColoCheckBox {
                    Layout.fillWidth: true
                    text: "Mode gage"
                    checked: AppController.gageMode
                    onClicked: AppController.gageMode = checked
                }
                Label {
                    Layout.fillWidth: true
                    visible: AppController.gageMode
                    text: "Les gages se saisissent dans l’onglet Gages. "
                          + "Le « N° gage » de chaque phrase (onglet Phrases) renvoie à ce tableau."
                    color: Theme.textDim
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: !AppController.gageMode
                    Label { text: "HP de départ"; color: Theme.textDim; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    ColoSpinBox {
                        from: 1; to: 100
                        value: AppController.startHP
                        onValueModified: AppController.startHP = value
                    }
                }

                Label {
                    text: "Multiplicateurs de points"
                    visible: !AppController.gageMode
                    color: Theme.text
                    font.weight: Font.DemiBold
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                GridLayout {
                    Layout.fillWidth: true
                    visible: !AppController.gageMode
                    columns: 2
                    rowSpacing: 8
                    columnSpacing: 12
                    Label { text: "Ligne"; color: Theme.textDim; Layout.fillWidth: true; elide: Text.ElideRight }
                    ColoSpinBox {
                        from: 1; to: 99
                        value: AppController.multipliers.line
                        onValueModified: AppController.setMultiplier("line", value)
                    }
                    Label { text: "Colonne"; color: Theme.textDim; Layout.fillWidth: true; elide: Text.ElideRight }
                    ColoSpinBox {
                        from: 1; to: 99
                        value: AppController.multipliers.column
                        onValueModified: AppController.setMultiplier("column", value)
                    }
                    Label { text: "Diagonale"; color: Theme.textDim; Layout.fillWidth: true; elide: Text.ElideRight }
                    ColoSpinBox {
                        from: 1; to: 99
                        value: AppController.multipliers.diagonal
                        onValueModified: AppController.setMultiplier("diagonal", value)
                    }
                    Label { text: "Grille complète"; color: Theme.textDim; Layout.fillWidth: true; elide: Text.ElideRight }
                    ColoSpinBox {
                        from: 1; to: 99
                        value: AppController.multipliers.full
                        onValueModified: AppController.setMultiplier("full", value)
                    }
                }

                Label { text: "Joueurs"; color: Theme.text; font.weight: Font.DemiBold; font.pixelSize: 14 }
                Repeater {
                    model: AppController.players
                    RowLayout {
                        Layout.fillWidth: true
                        Rectangle {
                            Layout.preferredWidth: 22
                            Layout.preferredHeight: 22
                            radius: 11
                            color: Theme.accentSoft
                            Label {
                                anchors.centerIn: parent
                                text: index + 1
                                color: Theme.accent
                                font.pixelSize: 11
                                font.weight: Font.Bold
                            }
                        }
                        ColoTextField {
                            Layout.fillWidth: true
                            text: modelData.name
                            hint: "Nom du joueur"
                            onEditingFinished: AppController.setPlayerName(index, text)
                        }
                        IconButton {
                            iconName: "trash"
                            danger: true
                            onClicked: AppController.removePlayer(index)
                        }
                    }
                }
                BingoButton {
                    Layout.fillWidth: true
                    text: "+ Ajouter un joueur"
                    onClicked: AppController.addPlayer()
                }

                BingoButton {
                    Layout.fillWidth: true
                    text: "Enregistrer la configuration"
                    primary: true
                    onClicked: AppController.saveConfig()
                }
            }
        }

        // MCP IA : desktop uniquement (localhost). Masqué sur Android.
        Rectangle {
            Layout.fillWidth: true
            visible: Qt.platform.os !== "android" && AppController.mcp
            implicitHeight: mcpSection.implicitHeight + 24
            radius: Theme.radiusLg
            color: Theme.surface
            border.color: Theme.outline

            ColumnLayout {
                id: mcpSection
                anchors.fill: parent
                anchors.margins: Theme.pad
                spacing: Theme.gap

                Label {
                    text: "MCP IA"
                    color: Theme.text
                    font.weight: Font.DemiBold
                    font.pixelSize: 14
                }
                Label {
                    Layout.fillWidth: true
                    text: "Serveur local pour Cursor / Claude Desktop (localhost). "
                          + "Activez, copiez la config, collez-la dans mcp.json."
                    color: Theme.textDim
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.gap
                    Label {
                        Layout.fillWidth: true
                        text: "Activer le serveur"
                        color: Theme.text
                        wrapMode: Text.WordWrap
                    }
                    Switch {
                        id: mcpSwitch
                        checked: AppController.mcp ? AppController.mcp.enabled : false
                        onToggled: {
                            if (AppController.mcp)
                                AppController.mcp.enabled = checked
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: AppController.mcp && AppController.mcp.running
                             && AppController.mcp.url.length > 0
                    text: "URL · " + (AppController.mcp ? AppController.mcp.url : "")
                    color: Theme.accent
                    font.pixelSize: 12
                    wrapMode: Text.WrapAnywhere
                }
                Label {
                    Layout.fillWidth: true
                    visible: AppController.mcp && AppController.mcp.enabled
                             && !AppController.mcp.running
                    text: "Démarrage en cours ou échec d’écoute — vérifiez le port."
                    color: Theme.warning
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: AppController.mcp && AppController.mcp.enabled
                    Label {
                        text: "Port"
                        color: Theme.textDim
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    ColoSpinBox {
                        from: 1024
                        to: 65535
                        value: AppController.mcp ? AppController.mcp.port : 4546
                        onValueModified: {
                            if (AppController.mcp)
                                AppController.mcp.port = value
                        }
                    }
                }

                BingoButton {
                    Layout.fillWidth: true
                    text: "Copier config"
                    primary: true
                    enabled: AppController.mcp && AppController.mcp.enabled
                    onClicked: {
                        if (AppController.mcp)
                            AppController.mcp.copyConfigToClipboard()
                    }
                }
                BingoButton {
                    Layout.fillWidth: true
                    text: "Régénérer le token"
                    enabled: AppController.mcp && AppController.mcp.enabled
                    onClicked: {
                        if (AppController.mcp)
                            AppController.mcp.regenerateToken()
                    }
                }
            }
        }

        // OpenSubtitles : clé API locale (PC). Jamais sync Nostr.
        Rectangle {
            id: osCard
            Layout.fillWidth: true
            visible: Qt.platform.os !== "android" && AppController.film
            implicitHeight: osSection.implicitHeight + 24
            radius: Theme.radiusLg
            color: Theme.surface
            border.color: Theme.outline

            Component.onCompleted: {
                if (AppController.film)
                    osApiKeyField.text = AppController.film.apiKey
            }
            Connections {
                target: AppController.film
                function onApiKeyChanged() {
                    if (AppController.film && !osApiKeyField.activeFocus)
                        osApiKeyField.text = AppController.film.apiKey
                }
            }

            ColumnLayout {
                id: osSection
                anchors.fill: parent
                anchors.margins: Theme.pad
                spacing: Theme.gap

                Label {
                    text: "OpenSubtitles"
                    color: Theme.text
                    font.weight: Font.DemiBold
                    font.pixelSize: 14
                }
                Label {
                    Layout.fillWidth: true
                    text: "Clé API consommateur pour Bingo film (stockée sur cet appareil)."
                    color: Theme.textDim
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
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
                        id: osSourceCombo
                        Layout.fillWidth: true
                        model: [
                            { code: "auto", label: "Auto (.com puis .org)" },
                            { code: "com", label: "opensubtitles.com (API)" },
                            { code: "org", label: "opensubtitles.org (web)" }
                        ]
                        textRole: "label"
                        Component.onCompleted: {
                            if (!AppController.film)
                                return
                            const s = (AppController.film.source || "auto").toLowerCase()
                            let idx = 0
                            for (let i = 0; i < model.length; ++i) {
                                if (model[i].code === s) {
                                    idx = i
                                    break
                                }
                            }
                            currentIndex = idx
                        }
                        onActivated: {
                            if (AppController.film && currentIndex >= 0)
                                AppController.film.source = model[currentIndex].code
                        }
                    }
                }

                ColoTextField {
                    id: osApiKeyField
                    Layout.fillWidth: true
                    hint: "Clé API (.com)"
                    echoMode: TextInput.Password
                }

                BingoButton {
                    Layout.fillWidth: true
                    text: "Tester / sauver"
                    primary: true
                    onClicked: {
                        if (!AppController.film)
                            return
                        AppController.film.setApiKey(osApiKeyField.text.trim())
                        if (AppController.film.hasApiKey)
                            AppController.film.clearError()
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: AppController.film && AppController.film.hasApiKey
                    text: "Clé enregistrée — tu peux chercher un film depuis Phrases."
                    color: Theme.success
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    text: "Login free (optionnel) — quotas download .com plus élevés."
                    color: Theme.textDim
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }

                ColoTextField {
                    id: osUserField
                    Layout.fillWidth: true
                    hint: "Nom d’utilisateur"
                    Component.onCompleted: {
                        if (AppController.film)
                            text = AppController.film.username || ""
                    }
                }

                ColoTextField {
                    id: osPassField
                    Layout.fillWidth: true
                    hint: "Mot de passe"
                    echoMode: TextInput.Password
                }

                BingoButton {
                    Layout.fillWidth: true
                    text: AppController.film && AppController.film.loggedIn
                          ? "Reconnecter (JWT)"
                          : "Connexion free"
                    enabled: AppController.film && !AppController.film.busy
                    onClicked: {
                        if (!AppController.film)
                            return
                        AppController.film.username = osUserField.text.trim()
                        AppController.film.login(osPassField.text)
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: AppController.film && AppController.film.loggedIn
                    text: "Compte connecté (JWT enregistré localement)."
                    color: Theme.success
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    text: '<a href="https://www.opensubtitles.com/en/consumers">Créer une clé sur opensubtitles.com → API consumers</a>'
                    color: Theme.accent
                    font.pixelSize: 12
                    textFormat: Text.RichText
                    wrapMode: Text.WordWrap
                    onLinkActivated: function (link) { Qt.openUrlExternally(link) }
                }
            }
        }

        Item { Layout.preferredHeight: Theme.pad * 2 }
    }
}
