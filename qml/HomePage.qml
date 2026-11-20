// Домашняя страница Crumb (новая вкладка): часы и приветствие, поиск,
// быстрые сайты в печеньках, погода, заметки. Всё настраивается прямо на
// странице — режим редактирования, как на телефоне.

import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import Crumb.Ui

Item {
    id: home

    signal navigate(string text)
    signal openSettings()

    property bool editMode: false
    property date now: new Date()
    clip: true

    Timer { interval: 1000; repeat: true; running: home.visible; onTriggered: home.now = new Date() }

    readonly property string greeting: {
        const h = now.getHours()
        return h >= 5 && h < 12 ? qsTr("Доброе утро")
             : h >= 12 && h < 17 ? qsTr("Добрый день")
             : h >= 17 && h < 23 ? qsTr("Добрый вечер") : qsTr("Доброй ночи")
    }

    // ================================================================ фон
    readonly property string bgMode: Settings.homeBackground
    readonly property url bgImage: bgMode === "wallpaper" && Theme.wallpaperPath.length
                                   ? "file:///" + Theme.wallpaperPath.replace(/\\/g, "/")
                                   : (bgMode === "image" && Settings.homeImage.length
                                      ? "file:///" + Settings.homeImage.replace(/\\/g, "/") : "")

    // Выбранный градиент: «Цвета темы» или готовый набор (основа + цвета пятен).
    readonly property var gradientPreset: Gradients.find(Settings.homeGradient)
    readonly property bool themeGradient: gradientPreset.colors.length === 0
    readonly property var blobColors: themeGradient
        ? [Theme.primaryContainer, Theme.tertiaryContainer, Theme.secondaryContainer, Theme.inversePrimary]
        : gradientPreset.colors.slice(1).concat([gradientPreset.colors[1]])

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop {
                position: 0
                color: home.bgMode === "gradient" && !home.themeGradient ? home.gradientPreset.colors[0] : Theme.surface
            }
            GradientStop {
                position: 1
                color: home.bgMode === "solid" ? Theme.surface
                     : home.bgMode === "gradient" && !home.themeGradient ? home.gradientPreset.colors[1] : Theme.surfaceContainer
            }
        }
    }

    // Анимированный градиент: большие мягкие пятна цветов темы медленно плавают.
    Item {
        id: blobs
        anchors.fill: parent
        visible: false
        property real t: 0
        NumberAnimation on t {
            from: 0; to: Math.PI * 2; duration: 60000; loops: Animation.Infinite
            running: home.visible && home.bgMode === "gradient" && Prefs.animations
        }
        Repeater {
            model: [[0.20, 0.25, 0.55, 1.0], [0.80, 0.30, 0.50, 1.3],
                    [0.55, 0.85, 0.60, 0.8], [0.10, 0.90, 0.35, 1.6]]
            Rectangle {
                required property var modelData
                required property int index
                readonly property real s: Math.max(home.width, home.height) * modelData[2]
                width: s; height: s; radius: s / 2
                x: home.width * modelData[0] - s / 2 + Math.sin(blobs.t * modelData[3]) * home.width * 0.08
                y: home.height * modelData[1] - s / 2 + Math.cos(blobs.t * modelData[3] * 0.8) * home.height * 0.08
                color: home.blobColors[index % home.blobColors.length]
                opacity: home.themeGradient ? 0.75 : 0.9
                Behavior on color { ColorAnimation { duration: Prefs.dur(600) } }
            }
        }
    }
    MultiEffect {
        anchors.fill: blobs
        source: blobs
        visible: home.bgMode === "gradient"
        blurEnabled: true
        blur: 1.0
        blurMax: 64
        autoPaddingEnabled: false
    }

    Rectangle {
        anchors.fill: parent
        visible: home.bgMode === "gradient" && !home.themeGradient
        color: Theme.surface
        opacity: 0.18 + Settings.homeDim * 0.6
    }

    // Обои или своя картинка, с размытием
    Image {
        id: picture
        anchors.fill: parent
        visible: false
        source: home.bgImage
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        sourceSize: Qt.size(1920, 1080)
    }
    MultiEffect {
        anchors.fill: picture
        source: picture
        visible: home.bgImage.toString().length > 0 && picture.status === Image.Ready
        blurEnabled: Settings.homeBlur > 0
        blur: Math.min(1, Settings.homeBlur / 60)
        blurMax: 64
        autoPaddingEnabled: false
    }
    // Затемнение для читаемости текста поверх картинки
    Rectangle {
        anchors.fill: parent
        visible: home.bgImage.toString().length > 0
        color: Theme.surface
        opacity: Settings.homeDim
    }

    // ============================================================ контент
    Flickable {
        id: flick
        anchors.fill: parent
        contentHeight: Math.max(height, column.implicitHeight + 160)
        bottomMargin: home.editMode ? editPanel.height + 24 : 0
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        ColumnLayout {
            id: column
            width: Math.min(flick.width - 48, 980)
            x: (flick.width - width) / 2
            y: Math.max(40, (flick.height - implicitHeight) / 2 - 40)
            spacing: 14
            // Появление страницы: выплывает снизу
            opacity: Prefs.animations ? 0 : 1
            transform: Translate { id: rise; y: Prefs.animations ? 28 : 0 }
            Component.onCompleted: if (Prefs.animations) homeEnter.start()
            ParallelAnimation {
                id: homeEnter
                NumberAnimation { target: column; property: "opacity"; to: 1; duration: Prefs.dur(380); easing.type: Easing.OutQuad }
                NumberAnimation { target: rise; property: "y"; to: 0; duration: Prefs.dur(560); easing.type: Easing.OutCubic }
            }

            // ---- часы и приветствие
            ColumnLayout {
                Layout.alignment: Qt.AlignHCenter
                visible: Settings.homeShowClock || Settings.homeShowGreeting
                spacing: 0
                CText {
                    Layout.alignment: Qt.AlignHCenter
                    visible: Settings.homeShowClock
                    text: Qt.formatTime(home.now, "HH:mm")
                    size: 92
                    font.weight: Font.Light
                    font.letterSpacing: -3
                    color: Theme.onSurface
                }
                CText {
                    Layout.alignment: Qt.AlignHCenter
                    visible: Settings.homeShowGreeting
                    text: home.greeting
                    size: 26
                    font.weight: Font.Medium
                    color: Theme.primary
                }
                CText {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 2
                    visible: Settings.homeShowClock
                    text: Qt.locale("ru_RU").toString(home.now, "dddd, d MMMM")
                    size: 15
                    color: Theme.onSurfaceVariant
                }
            }

            // ---- поиск
            Rectangle {
                id: searchBox
                visible: Settings.homeShowSearch
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: Math.min(column.width, 680)
                Layout.preferredHeight: 60
                Layout.topMargin: 10
                radius: 30
                color: searchInput.activeFocus ? Theme.surfaceContainerLowest : Qt.alpha(Theme.surfaceContainerHigh, 0.92)
                border.width: searchInput.activeFocus ? 2 : 0
                border.color: Theme.primary
                Behavior on color { ColorAnimation { duration: Prefs.dur(180) } }

                readonly property var engines: Engines.list
                readonly property var engine: Engines.find(Settings.searchEngine)

                CookieButton {
                    id: engineBtn
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    size: 40
                    letter: searchBox.engine.letter
                    iconSource: searchBox.engine.icon
                    toolTip: qsTr("Поисковик: %1").arg(searchBox.engine.name)
                    onClicked: engineMenu.popup()
                    ThemedMenu {
                        id: engineMenu
                        Repeater {
                            model: searchBox.engines
                            ThemedMenuItem {
                                required property var modelData
                                text: modelData.name
                                iconSource: modelData.icon
                                checked: modelData.id === Settings.searchEngine
                                onTriggered: Settings.searchEngine = modelData.id
                            }
                        }
                    }
                }
                TextInput {
                    id: searchInput
                    anchors.left: engineBtn.right
                    anchors.leftMargin: 14
                    anchors.right: goBtn.left
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    clip: true
                    selectByMouse: true
                    font.family: Prefs.font
                    font.pixelSize: Prefs.px(17)
                    color: Theme.onSurface
                    selectionColor: Theme.primaryContainer
                    selectedTextColor: Theme.onPrimaryContainer
                    onAccepted: if (text.trim().length) { home.navigate(text.trim()); text = "" }
                    CText {
                        anchors.fill: parent
                        verticalAlignment: Text.AlignVCenter
                        visible: searchInput.text.length === 0
                        text: qsTr("Искать в %1 или ввести адрес").arg(searchBox.engine.name)
                        color: Theme.onSurfaceVariant
                        size: 17
                    }
                }
                CookieButton {
                    id: goBtn
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    size: 40
                    variant: "filled"
                    icon: ""
                    toolTip: qsTr("Найти")
                    onClicked: searchInput.accepted()
                }
            }

            // ---- быстрые сайты
            Item {
                id: sitesArea
                visible: Settings.homeShowSites
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 14
                readonly property int columns: Math.max(3, Math.min(Settings.homeColumns,
                                                  Math.floor(column.width / cellW)))
                readonly property real tile: Settings.homeTileSize
                readonly property real cellW: tile + 44
                readonly property real cellH: tile * Prefs.cookieScale + 44
                implicitWidth: columns * cellW
                implicitHeight: Math.ceil((sitesModel.count + 1) / columns) * cellH

                ListModel { id: sitesModel }
                function load() {
                    sitesModel.clear()
                    const list = Settings.homeSites || []
                    for (let i = 0; i < list.length; ++i)
                        sitesModel.append({ title: String(list[i].title), url: String(list[i].url) })
                }
                function save() {
                    const out = []
                    for (let i = 0; i < sitesModel.count; ++i)
                        out.push({ title: sitesModel.get(i).title, url: sitesModel.get(i).url })
                    Settings.homeSites = out
                }
                Component.onCompleted: load()
                Connections {
                    target: Settings
                    function onSettingChanged(key) { if (key === "homeSites" && !sitesArea.saving) sitesArea.load() }
                }
                property bool saving: false
                function commit() { saving = true; save(); Qt.callLater(() => saving = false) }
                // Вызывать отсюда, а не из плитки: remove() уничтожает плитку, и код
                // после него в её обработчике уже не выполнился бы (сайт «возвращался»).
                function removeSite(i) { sitesModel.remove(i); commit() }
                function updateSite(i, title, url) {
                    sitesModel.set(i, { title: title, url: url })
                    commit()
                }

                function hostOf(url) {
                    const m = /^[a-z]+:\/\/([^\/?#]+)/i.exec(url)
                    return m ? m[1] : url
                }

                Repeater {
                    model: sitesModel
                    delegate: Item {
                        id: cell
                        required property int index
                        required property string title
                        required property string url
                        width: sitesArea.cellW
                        height: sitesArea.cellH
                        x: (index % sitesArea.columns) * sitesArea.cellW
                        y: Math.floor(index / sitesArea.columns) * sitesArea.cellH
                        Behavior on x { enabled: Prefs.animations && !drag.active; SpringAnimation { spring: Prefs.spring(3); damping: 0.3 } }
                        Behavior on y { enabled: Prefs.animations && !drag.active; SpringAnimation { spring: Prefs.spring(3); damping: 0.3 } }
                        z: drag.active ? 10 : 1

                        Item {
                            id: tileItem
                            width: parent.width
                            height: parent.height
                            // Плитки появляются волной, одна за другой
                            opacity: Prefs.animations ? 0 : 1
                            scale: Prefs.animations ? 0.6 : 1
                            Component.onCompleted: if (Prefs.animations) tileEnter.start()
                            SequentialAnimation {
                                id: tileEnter
                                PauseAnimation { duration: Prefs.dur(160 + cell.index * 45) }
                                ParallelAnimation {
                                    NumberAnimation { target: tileItem; property: "opacity"; to: 1; duration: Prefs.dur(220) }
                                    NumberAnimation { target: tileItem; property: "scale"; to: 1; duration: Prefs.dur(460); easing.type: Easing.OutBack; easing.overshoot: 1.6 }
                                }
                            }
                            // В режиме редактирования значки «покачиваются», как на телефоне.
                            rotation: home.editMode && Prefs.animations ? wobble.angle : 0
                            QtObject { id: wobble; property real angle: 0 }
                            SequentialAnimation {
                                running: home.editMode && Prefs.animations
                                loops: Animation.Infinite
                                NumberAnimation { target: wobble; property: "angle"; to: 2.5; duration: 140 + (cell.index % 3) * 30 }
                                NumberAnimation { target: wobble; property: "angle"; to: -2.5; duration: 280 }
                                NumberAnimation { target: wobble; property: "angle"; to: 0; duration: 140 }
                            }

                            CookieButton {
                                anchors.horizontalCenter: parent.horizontalCenter
                                y: 8
                                size: sitesArea.tile
                                labelPosition: "below"
                                labelWidth: sitesArea.cellW - 8
                                text: cell.title
                                letter: cell.title.length ? cell.title[0].toUpperCase() : "?"
                                iconSource: "https://" + sitesArea.hostOf(cell.url) + "/favicon.ico"
                                variant: "surface"
                                toolTip: cell.url
                                // В режиме правки — изменить название и адрес
                                onClicked: home.editMode ? sitePopup.openFor(cell.index, cell.title, cell.url)
                                                         : home.navigate(cell.url)
                            }
                            CookieButton {
                                visible: home.editMode
                                z: 5
                                x: parent.width / 2 + sitesArea.tile * Prefs.cookieScale / 2 - 20
                                y: -2
                                size: 30
                                variant: "filled"
                                color: Theme.error
                                contentColor: Theme.onError
                                icon: ""
                                toolTip: qsTr("Убрать")
                                onClicked: sitesArea.removeSite(cell.index)
                            }
                        }

                        // Перетаскивание: в режиме редактирования или долгим нажатием.
                        DragHandler {
                            id: drag
                            target: cell
                            enabled: home.editMode
                            onActiveChanged: {
                                if (active) return
                                const col = Math.max(0, Math.min(sitesArea.columns - 1, Math.round(cell.x / sitesArea.cellW)))
                                const row = Math.max(0, Math.round(cell.y / sitesArea.cellH))
                                const to = Math.max(0, Math.min(sitesModel.count - 1, row * sitesArea.columns + col))
                                const from = cell.index
                                // вернуть на сетку (x/y снова привязываются к индексу)
                                cell.x = Qt.binding(() => (cell.index % sitesArea.columns) * sitesArea.cellW)
                                cell.y = Qt.binding(() => Math.floor(cell.index / sitesArea.columns) * sitesArea.cellH)
                                if (to !== from) { sitesModel.move(from, to, 1); sitesArea.commit() }
                            }
                        }
                    }
                }

                // Плитка «Добавить»
                CookieButton {
                    readonly property int i: sitesModel.count
                    x: (i % sitesArea.columns) * sitesArea.cellW + (sitesArea.cellW - width) / 2
                    y: Math.floor(i / sitesArea.columns) * sitesArea.cellH + 8
                    size: sitesArea.tile
                    labelPosition: "below"
                    labelWidth: sitesArea.cellW - 8
                    variant: "outlined"
                    icon: ""
                    text: qsTr("Добавить")
                    onClicked: sitePopup.openFor(-1, "", "")
                }
            }

            // ---- виджеты: погода и заметки
            Flow {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 10
                Layout.preferredWidth: Math.min(column.width, (Settings.homeShowWeather ? 300 : 0) + (Settings.homeShowNotes ? 400 : 0) + 20)
                visible: Settings.homeShowWeather || Settings.homeShowNotes
                spacing: 18

                // Погода
                Rectangle {
                    visible: Settings.homeShowWeather
                    width: 290
                    height: 160
                    radius: 28
                    color: Qt.alpha(Theme.surfaceContainer, 0.88)

                    // город не задан — поле ввода прямо в карточке
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        visible: Settings.homeCity.length === 0
                        spacing: 10
                        CText { text: qsTr("Погода"); size: 18; font.weight: Font.DemiBold }
                        CText { text: qsTr("Любой город или село — начните вводить"); size: 13; color: Theme.onSurfaceVariant }
                        RowLayout {
                            spacing: 8
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 40
                                radius: 20
                                color: Theme.surfaceContainerHighest
                                TextInput {
                                    id: cityInput
                                    anchors.fill: parent
                                    anchors.leftMargin: 14
                                    anchors.rightMargin: 14
                                    verticalAlignment: TextInput.AlignVCenter
                                    font.family: Prefs.font
                                    font.pixelSize: Prefs.px(14)
                                    color: Theme.onSurface
                                    clip: true
                                    // Enter: первая подсказка, а если их нет — поиск по тексту
                                    // (с поправкой опечаток и поиском сёл в OpenStreetMap).
                                    onAccepted: {
                                        if (Weather.suggestions.length > 0) Weather.choose(0)
                                        else if (text.trim().length) Settings.homeCity = text.trim()
                                    }
                                    onTextEdited: suggestTimer.restart()
                                    // Ушли из поля, ничего не выбрав, — подсказки убираем (с паузой,
                                    // чтобы успел сработать клик по подсказке).
                                    onActiveFocusChanged: if (!activeFocus) hideSuggest.restart()
                                    Timer { id: hideSuggest; interval: 300; onTriggered: if (!cityInput.activeFocus) Weather.clearSuggestions() }
                                    Keys.onEscapePressed: Weather.clearSuggestions()
                                    Timer {
                                        id: suggestTimer
                                        interval: 400
                                        onTriggered: Weather.suggest(cityInput.text)
                                    }
                                    CText {
                                        anchors.fill: parent
                                        verticalAlignment: Text.AlignVCenter
                                        visible: cityInput.text.length === 0
                                        text: qsTr("Город или село")
                                        color: Theme.onSurfaceVariant
                                    }
                                }
                                // Подсказки мест
                                Popup {
                                    id: suggestPopup
                                    y: parent.height + 6
                                    width: 340
                                    padding: 6
                                    visible: Weather.suggestions.length > 0 || Weather.suggesting
                                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
                                    background: Rectangle { color: Theme.surfaceContainerHigh; radius: 18; border.color: Theme.outlineVariant }
                                    contentItem: Column {
                                        spacing: 2
                                        CText {
                                            visible: Weather.suggesting && Weather.suggestions.length === 0
                                            text: qsTr("Ищем…")
                                            color: Theme.onSurfaceVariant
                                            leftPadding: 12
                                            topPadding: 8
                                            bottomPadding: 8
                                        }
                                        Repeater {
                                            model: Weather.suggestions
                                            Rectangle {
                                                required property var modelData
                                                required property int index
                                                width: 328
                                                height: 46
                                                radius: 12
                                                color: hov.hovered ? Qt.alpha(Theme.onSurface, 0.08) : "transparent"
                                                Column {
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    x: 12
                                                    width: parent.width - 24
                                                    CText { text: modelData.name; size: 14; font.weight: Font.Medium; elide: Text.ElideRight; width: parent.width }
                                                    CText { text: modelData.label; size: 11.5; color: Theme.onSurfaceVariant; elide: Text.ElideRight; width: parent.width }
                                                }
                                                HoverHandler { id: hov; cursorShape: Qt.PointingHandCursor }
                                                TapHandler { onTapped: Weather.choose(index) }
                                            }
                                        }
                                    }
                                }
                            }
                            CookieButton {
                                variant: "filled"; icon: ""; size: 40
                                toolTip: qsTr("Сохранить город")
                                onClicked: cityInput.accepted()
                            }
                        }
                    }

                    // город задан
                    Item {
                        anchors.fill: parent
                        anchors.margins: 20
                        visible: Settings.homeCity.length > 0
                        Text {
                            id: weatherIcon
                            visible: Weather.ready
                            text: Weather.emoji
                            font.family: "Segoe UI Emoji"
                            font.pixelSize: 54
                            anchors.right: parent.right
                            anchors.top: parent.top
                        }
                        CookieShape {
                            visible: Weather.loading && !Weather.ready
                            width: 36; height: 36
                            anchors.right: parent.right
                            color: Theme.primary
                            spin: 200
                        }
                        Column {
                            spacing: 2
                            CText {
                                text: Weather.ready ? (Weather.temperature > 0 ? "+" : "") + Weather.temperature + "°" : "—"
                                size: 44
                                font.weight: Font.Light
                            }
                            CText {
                                text: Weather.ready ? Weather.description : (Weather.error.length ? Weather.error : qsTr("Загрузка…"))
                                size: 14
                                color: Weather.error.length ? Theme.error : Theme.onSurface
                            }
                            CText {
                                text: Weather.ready ? Weather.city + " · ↑" + Weather.high + "° ↓" + Weather.low + "°" : Settings.homeCity
                                size: 13
                                color: Theme.onSurfaceVariant
                            }
                        }
                        CookieButton {
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            size: 28
                            variant: "surface"
                            icon: ""
                            toolTip: qsTr("Сменить город")
                            onClicked: Settings.homeCity = ""
                        }
                    }
                }

                // Заметки
                Rectangle {
                    visible: Settings.homeShowNotes
                    width: Math.min(column.width, 390)
                    height: 160
                    radius: 28
                    color: Qt.alpha(Theme.tertiaryContainer, 0.9)
                    CText {
                        id: notesTitle
                        x: 20; y: 16
                        text: qsTr("Заметки")
                        size: 16
                        font.weight: Font.DemiBold
                        color: Theme.onTertiaryContainer
                    }
                    Flickable {
                        anchors.fill: parent
                        anchors.topMargin: 44
                        anchors.margins: 20
                        contentHeight: notes.implicitHeight
                        clip: true
                        TextEdit {
                            id: notes
                            width: parent.width
                            wrapMode: TextEdit.Wrap
                            text: Settings.homeNotes
                            font.family: Prefs.font
                            font.pixelSize: Prefs.px(14)
                            color: Theme.onTertiaryContainer
                            selectByMouse: true
                            selectionColor: Theme.tertiary
                            onTextChanged: notesSave.restart()
                            CText {
                                visible: notes.text.length === 0 && !notes.activeFocus
                                text: qsTr("Запишите что-нибудь…")
                                color: Qt.alpha(Theme.onTertiaryContainer, 0.6)
                            }
                        }
                    }
                    Timer {   // сохраняем через полсекунды после последнего изменения
                        id: notesSave
                        interval: 500
                        onTriggered: if (notes.text !== Settings.homeNotes) Settings.homeNotes = notes.text
                    }
                }
            }
        }
    }

    // ============================================== кнопки и режим правки
    Row {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 22
        spacing: 12
        CookieButton {
            icon: ""
            variant: "surface"
            toolTip: qsTr("Все настройки")
            onClicked: home.openSettings()
        }
        CookieButton {
            icon: home.editMode ? "" : ""
            variant: home.editMode ? "filled" : "tonal"
            text: home.editMode ? qsTr("Готово") : qsTr("Настроить")
            onClicked: home.editMode = !home.editMode
        }
    }

    // Панель настройки страницы (как на телефоне): компактная полоса снизу.
    // Подробные ползунки (размытие, сетка, размер значков) — в «Настройках».
    Rectangle {
        id: editPanel
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width - 200, 760)
        height: editCol.implicitHeight + 24
        y: home.editMode ? parent.height - height - 16 : parent.height + 20
        Behavior on y { enabled: Prefs.animations; SpringAnimation { spring: Prefs.spring(3); damping: 0.32 } }
        visible: y < parent.height
        radius: 24
        color: Qt.alpha(Theme.surfaceContainerHigh, 0.97)
        border.color: Theme.outlineVariant

        component MiniChoice: CookieButton {
            size: 32
            labelPosition: "below"
            labelWidth: 62
        }

        ColumnLayout {
            id: editCol
            anchors.fill: parent
            anchors.margins: 12
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                CText { text: qsTr("Показывать"); size: 12; color: Theme.onSurfaceVariant; font.weight: Font.DemiBold; Layout.preferredWidth: 78 }
                Repeater {
                    model: [["homeShowClock", "\uE823", qsTr("Часы")], ["homeShowGreeting", "\uE76E", qsTr("Привет")],
                            ["homeShowSearch", "\uE721", qsTr("Поиск")], ["homeShowSites", "\uE80A", qsTr("Сайты")],
                            ["homeShowWeather", "\uE9CA", qsTr("Погода")], ["homeShowNotes", "\uE70B", qsTr("Заметки")]]
                    MiniChoice {
                        required property var modelData
                        icon: modelData[1]
                        text: modelData[2]
                        checked: Settings[modelData[0]]
                        onClicked: Settings[modelData[0]] = !Settings[modelData[0]]
                    }
                }
                Item { Layout.fillWidth: true }
                CookieButton {
                    size: 32
                    icon: "\uE713"
                    variant: "surface"
                    text: qsTr("Ещё…")
                    toolTip: qsTr("Размытие, сетка, размер значков и остальное")
                    onClicked: { home.editMode = false; home.openSettings() }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                CText { text: qsTr("Фон"); size: 12; color: Theme.onSurfaceVariant; font.weight: Font.DemiBold; Layout.preferredWidth: 78 }
                Repeater {
                    model: [["gradient", "\uE790", qsTr("Градиент")], ["solid", "\uE91F", qsTr("Цвет")],
                            ["wallpaper", "\uE7F4", qsTr("Обои")], ["image", "\uEB9F", qsTr("Картинка")]]
                    MiniChoice {
                        required property var modelData
                        icon: modelData[1]
                        text: modelData[2]
                        checked: Settings.homeBackground === modelData[0]
                        onClicked: modelData[0] === "image" ? imageDialog.open() : Settings.homeBackground = modelData[0]
                    }
                }
                // Градиенты — лентой в той же строке
                Flickable {
                    id: gradFlick
                    visible: Settings.homeBackground === "gradient"
                    Layout.fillWidth: true
                    Layout.preferredWidth: 100     // ширину даёт строка, а не длина ленты
                    Layout.preferredHeight: 66
                    Layout.leftMargin: 8
                    contentWidth: gradRow.width
                    contentHeight: height
                    flickableDirection: Flickable.HorizontalFlick
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    // Колёсико листает ленту вбок (WheelHandler внутри Flickable попадает в contentItem,
                    // поэтому обращаемся к ленте по id).
                    WheelHandler {
                        target: null
                        onWheel: (e) => {
                            const d = e.angleDelta.y !== 0 ? e.angleDelta.y : e.angleDelta.x
                            gradFlick.contentX = Math.max(0, Math.min(gradFlick.contentWidth - gradFlick.width, gradFlick.contentX - d))
                        }
                    }
                    Behavior on contentX { enabled: Prefs.animations && !gradFlick.dragging && !gradFlick.flicking; NumberAnimation { duration: Prefs.dur(180); easing.type: Easing.OutCubic } }
                    ScrollBar.horizontal: ScrollBar { policy: gradFlick.contentWidth > gradFlick.width ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff; height: 4 }
                    Row {
                        id: gradRow
                        spacing: 8
                        Repeater {
                            model: Gradients.list
                            GradientChoice { required property var modelData; preset: modelData }
                        }
                    }
                }
                Item { visible: Settings.homeBackground !== "gradient"; Layout.fillWidth: true }
            }
        }
    }

    // Печенька-образец градиента; отмеченная — с галочкой.
    component GradientChoice: CookieButton {
        property var preset
        readonly property bool selected: Settings.homeGradient === preset.id
        size: 32
        labelPosition: "below"
        labelWidth: 58
        text: preset.name
        gradientColors: preset.colors.length ? preset.colors
                        : [Theme.primaryContainer, Theme.tertiaryContainer, Theme.secondaryContainer]
        contentColor: "white"
        icon: selected ? "" : ""
        depth: selected ? Prefs.cookieDepth * 2 : Prefs.cookieDepth
        onClicked: { Settings.homeGradient = preset.id; Settings.homeBackground = "gradient" }
    }

    FileDialog {
        id: imageDialog
        title: qsTr("Картинка для фона")
        nameFilters: [qsTr("Изображения (*.jpg *.jpeg *.png *.webp *.bmp)")]
        onAccepted: {
            Settings.homeImage = Settings.localFile(selectedFile)
            Settings.homeBackground = "image"
        }
    }

    // Добавление (index = -1) и правка сайта
    Popup {
        id: sitePopup
        property int index: -1
        function openFor(i, title, url) {
            index = i
            open()
            titleField.text = title
            urlField.text = url
            urlField.forceActiveFocus()
        }
        anchors.centerIn: parent
        width: 420
        modal: true
        padding: 22
        background: Rectangle { color: Theme.surfaceContainerHigh; radius: 28 }
        function add() {
            let url = urlField.text.trim()
            if (!url.length) return
            if (!/^[a-z]+:\/\//i.test(url)) url = "https://" + url
            let title = titleField.text.trim()
            if (!title.length) title = sitesArea.hostOf(url).replace(/^www\./, "")
            if (index >= 0) sitesArea.updateSite(index, title, url)
            else { sitesModel.append({ title: title, url: url }); sitesArea.commit() }
            close()
        }

        component Field: Rectangle {
            property alias input: inp
            property string placeholder
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            radius: 23
            color: Theme.surfaceContainerHighest
            border.width: inp.activeFocus ? 2 : 0
            border.color: Theme.primary
            TextInput {
                id: inp
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                verticalAlignment: TextInput.AlignVCenter
                font.family: Prefs.font
                font.pixelSize: Prefs.px(15)
                color: Theme.onSurface
                clip: true
                selectByMouse: true
                onAccepted: sitePopup.add()
                CText {
                    anchors.fill: parent
                    verticalAlignment: Text.AlignVCenter
                    visible: inp.text.length === 0
                    text: parent.parent.placeholder
                    color: Theme.onSurfaceVariant
                }
            }
        }

        contentItem: ColumnLayout {
            spacing: 12
            CText { text: sitePopup.index >= 0 ? qsTr("Изменить сайт") : qsTr("Новый сайт"); size: 22; font.weight: Font.DemiBold }
            Field { id: urlFieldBox; placeholder: qsTr("Адрес, например github.com") }
            Field { id: titleFieldBox; placeholder: qsTr("Название (необязательно)") }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                Layout.topMargin: 8
                spacing: 18
                CookieButton {
                    visible: sitePopup.index >= 0
                    icon: "\uE74D"; text: qsTr("Удалить")
                    color: Theme.errorContainer; contentColor: Theme.onErrorContainer
                    onClicked: { sitesArea.removeSite(sitePopup.index); sitePopup.close() }
                }
                CookieButton { variant: "tonal"; icon: "\uE711"; text: qsTr("Отмена"); onClicked: sitePopup.close() }
                CookieButton {
                    variant: "filled"
                    icon: sitePopup.index >= 0 ? "\uE73E" : "\uE710"
                    text: sitePopup.index >= 0 ? qsTr("Сохранить") : qsTr("Добавить")
                    onClicked: sitePopup.add()
                }
            }
        }
        property alias urlField: urlFieldBox.input
        property alias titleField: titleFieldBox.input
    }
}
