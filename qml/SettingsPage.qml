// Настройки Crumb: разделы слева, поиск по ним, все изменения видны сразу.
// Каждая строка — обычный компонент, привязанный к Settings / Theme / Prefs.

import QtQuick
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Controls.Basic
import Crumb
import Crumb.Ui

Item {
    id: page

    signal pickColorFromScreen()

    property string query: searchField.text.trim().toLowerCase()
    function matches(text) { return query === "" || text.toLowerCase().indexOf(query) >= 0 }

    Rectangle { anchors.fill: parent; color: Theme.surface }

    // ======================================================== компоненты
    // Раздел: заголовок со значком и строки; скрывается, если поиск ничего не нашёл.
    component Section: ColumnLayout {
        id: sec
        property string title
        property string icon
        default property alias rows: rowsCol.data
        readonly property bool titleHit: page.query !== "" && page.matches(title)
        property bool shown: {
            if (page.query === "" || titleHit) return true
            for (let i = 0; i < rowsCol.children.length; ++i)
                if (rowsCol.children[i].shown) return true
            return false
        }
        visible: shown
        Layout.fillWidth: true
        spacing: 4

        RowLayout {
            Layout.topMargin: 26
            Layout.bottomMargin: 6
            spacing: 12
            CookieShape {
                width: 34; height: 34
                color: Theme.secondaryContainer
                Text {
                    anchors.centerIn: parent
                    text: sec.icon
                    font.family: Theme.iconFont
                    font.pixelSize: 15
                    color: Theme.onSecondaryContainer
                }
            }
            CText { text: sec.title; size: 22; font.weight: Font.DemiBold }
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: rowsCol.implicitHeight + 24
            radius: 24
            color: Theme.surfaceContainerLow
            ColumnLayout {
                id: rowsCol
                property string sectionTitle: sec.title
                property bool sectionHit: sec.titleHit
                anchors.fill: parent
                anchors.margins: 12
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 2
            }
        }
    }

    // Базовая строка: подпись, пояснение и элемент справа/снизу.
    component Row_: ColumnLayout {
        property string label
        property string hint: ""
        property string keywords: ""
        readonly property bool shown: page.query === "" || (parent && parent.sectionHit) ||
                                       page.matches(label + " " + hint + " " + keywords)
        visible: shown
        Layout.fillWidth: true
        Layout.topMargin: 6
        Layout.bottomMargin: 6
        spacing: 4
    }

    component SSwitch: Row_ {
        id: sw
        property bool checked
        signal toggled(bool on)
        CookieSwitch {
            Layout.fillWidth: true
            text: sw.label
            description: sw.hint
            checked: sw.checked
            onToggled: (on) => sw.toggled(on)
        }
    }

    component SSlider: Row_ {
        id: sl
        property real from: 0
        property real to: 1
        property real value: 0
        property real stepSize: 0
        property string suffix: ""
        property var formatValue: null
        signal moved(real v)
        RowLayout {
            Layout.fillWidth: true
            CText { text: sl.label; size: 15; Layout.fillWidth: true }
            CText {
                text: sl.formatValue ? sl.formatValue(sl.value)
                                     : (sl.stepSize >= 1 ? Math.round(sl.value) : sl.value.toFixed(2)) + sl.suffix
                size: 13
                color: Theme.primary
                font.weight: Font.DemiBold
            }
        }
        CText { visible: sl.hint.length > 0; text: sl.hint; size: 12.5; color: Theme.onSurfaceVariant; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        CookieSlider {
            Layout.fillWidth: true
            from: sl.from; to: sl.to; value: sl.value; stepSize: sl.stepSize
            suffix: sl.suffix; formatValue: sl.formatValue
            onMoved: (v) => sl.moved(v)
        }
    }

    // Выбор одного варианта: печеньки с подписями.
    component SChoice: Row_ {
        id: ch
        property var options: []        // [{ value, label, icon }]
        property var current
        signal chosen(var value)
        CText { text: ch.label; size: 15 }
        CText { visible: ch.hint.length > 0; text: ch.hint; size: 12.5; color: Theme.onSurfaceVariant; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        Flow {
            Layout.fillWidth: true
            Layout.topMargin: 6
            spacing: 18
            Repeater {
                model: ch.options
                CookieButton {
                    required property var modelData
                    size: 44
                    icon: modelData.icon || ""
                    letter: modelData.letter || ""
                    iconSource: modelData.iconSource || ""
                    text: modelData.label
                    labelPosition: "below"
                    checked: ch.current === modelData.value
                    onClicked: ch.chosen(modelData.value)
                }
            }
        }
    }

    // Поле ввода текста
    component SField: Row_ {
        id: fld
        property string text
        property string placeholder: ""
        property bool multiline: false
        signal edited(string text)
        CText { text: fld.label; size: 15 }
        CText { visible: fld.hint.length > 0; text: fld.hint; size: 12.5; color: Theme.onSurfaceVariant; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: fld.multiline ? 110 : 42
            radius: fld.multiline ? 16 : 21
            color: Theme.surfaceContainerHighest
            border.width: (fld.multiline ? te.activeFocus : ti.activeFocus) ? 2 : 0
            border.color: Theme.primary
            TextInput {
                id: ti
                visible: !fld.multiline
                anchors.fill: parent
                anchors.leftMargin: 16; anchors.rightMargin: 16
                verticalAlignment: TextInput.AlignVCenter
                text: fld.text
                font.family: Prefs.font
                font.pixelSize: Prefs.px(14)
                color: Theme.onSurface
                selectionColor: Theme.primaryContainer
                clip: true
                selectByMouse: true
                onEditingFinished: if (text !== fld.text) fld.edited(text)
                CText {
                    anchors.fill: parent
                    verticalAlignment: Text.AlignVCenter
                    visible: ti.text.length === 0
                    text: fld.placeholder
                    color: Theme.onSurfaceVariant
                }
            }
            Flickable {
                visible: fld.multiline
                anchors.fill: parent
                anchors.margins: 12
                contentHeight: te.implicitHeight
                clip: true
                TextEdit {
                    id: te
                    width: parent.width
                    text: fld.text
                    wrapMode: TextEdit.Wrap
                    font.family: Prefs.font
                    font.pixelSize: Prefs.px(14)
                    color: Theme.onSurface
                    selectByMouse: true
                    selectionColor: Theme.primaryContainer
                    onActiveFocusChanged: if (!activeFocus && text !== fld.text) fld.edited(text)
                }
            }
        }
    }

    // Действие: печенька с подписью
    component SAction: Row_ {
        id: act
        property string icon
        property string buttonText
        property string variant: "tonal"
        property color buttonColor: "transparent"
        signal triggered()
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                CText { text: act.label; size: 15; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                CText { visible: act.hint.length > 0; text: act.hint; size: 12.5; color: Theme.onSurfaceVariant; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            }
            CookieButton {
                icon: act.icon
                text: act.buttonText
                variant: act.variant
                color: act.buttonColor.a > 0 ? act.buttonColor
                     : (variant === "filled" ? Theme.primary : Theme.secondaryContainer)
                contentColor: act.buttonColor.a > 0 ? Theme.onError
                            : (variant === "filled" ? Theme.onPrimary : Theme.onSecondaryContainer)
                onClicked: act.triggered()
            }
        }
    }

    // Образец цвета-печенька
    component Swatch: CookieButton {
        property color swatch
        property bool selected: false
        size: 38
        depth: selected ? Prefs.cookieDepth * 2 : Prefs.cookieDepth
        color: swatch
        contentColor: "white"
        icon: selected ? "" : ""
    }

    // ========================================================== разметка
    readonly property var sections: [
        { title: qsTr("Аккаунт"), icon: "\uE77B" },
        { title: qsTr("Цвета"), icon: "" }, { title: qsTr("Печеньки"), icon: "" },
        { title: qsTr("Вкладки"), icon: "" }, { title: qsTr("Панель"), icon: "" },
        { title: qsTr("Шрифты"), icon: "" }, { title: qsTr("Анимации"), icon: "" },
        { title: qsTr("Домашняя страница"), icon: "" }, { title: qsTr("Курсор и звуки"), icon: "" },
        { title: qsTr("Rewind"), icon: "" }, { title: qsTr("История и пароли"), icon: "\uE823" }, { title: qsTr("Приватность"), icon: "" },
        { title: qsTr("Поисковик"), icon: "" }, { title: qsTr("Загрузки"), icon: "" },
        { title: qsTr("Горячие клавиши"), icon: "" }, { title: qsTr("Резервная копия и сброс"), icon: "" },
        { title: qsTr("О Crumb"), icon: "" }
    ]

    // ---- левая колонка: поиск и разделы
    Rectangle {
        id: nav
        width: 270
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        color: Theme.surfaceContainerLow

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 6

            CText { text: qsTr("Настройки"); size: 28; font.weight: Font.Bold; Layout.bottomMargin: 6 }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                radius: 22
                color: Theme.surfaceContainerHighest
                border.width: searchField.activeFocus ? 2 : 0
                border.color: Theme.primary
                Text {
                    id: searchGlyph
                    anchors.left: parent.left; anchors.leftMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    text: ""; font.family: Theme.iconFont; font.pixelSize: 14
                    color: Theme.onSurfaceVariant
                }
                TextInput {
                    id: searchField
                    anchors.left: searchGlyph.right; anchors.leftMargin: 10
                    anchors.right: parent.right; anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    font.family: Prefs.font
                    font.pixelSize: Prefs.px(14)
                    color: Theme.onSurface
                    clip: true
                    selectByMouse: true
                    CText {
                        anchors.fill: parent
                        verticalAlignment: Text.AlignVCenter
                        visible: searchField.text.length === 0
                        text: qsTr("Поиск по настройкам")
                        color: Theme.onSurfaceVariant
                    }
                }
            }
            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.topMargin: 8
                contentHeight: navCol.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                Column {
                    id: navCol
                    width: parent.width
                    spacing: 4
                    Repeater {
                        model: page.sections
                        CookieButton {
                            required property var modelData
                            required property int index
                            readonly property Item target: content.children[index] || null
                            visible: !target || target.shown
                            size: 34
                            icon: modelData.icon
                            text: modelData.title
                            checked: page.currentSection === index
                            variant: "surface"
                            onClicked: if (target) page.scrollTo(Math.min(target.y, flick.contentHeight - flick.height))
                        }
                    }
                }
            }
        }
    }

    // Плавная прокрутка к разделу (при выключенных анимациях — сразу)
    function scrollTo(y) {
        scrollAnim.stop()
        if (!Prefs.animations) { flick.contentY = y; return }
        scrollAnim.to = Math.max(0, y)
        scrollAnim.start()
    }
    NumberAnimation { id: scrollAnim; target: flick; property: "contentY"; duration: Prefs.dur(520); easing.type: Easing.OutCubic }

    property int currentSection: {
        let cur = 0
        for (let i = 0; i < content.children.length; ++i) {
            const c = content.children[i]
            if (c.visible && c.y <= flick.contentY + 40) cur = i
        }
        return cur
    }

    // ---- правая колонка: разделы
    Flickable {
        id: flick
        anchors.left: nav.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        contentHeight: content.implicitHeight + 80
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: content
            width: Math.min(flick.width - 64, 760)
            x: 32
            spacing: 0

            // ------------------------------------------------------ Аккаунт
            Section {
                title: qsTr("Аккаунт"); icon: "\uE77B"

                // Нет Client ID: подсказка, как создать приложение GitHub
                Row_ {
                    visible: shown && !Sync.configured && Settings.syncClientId.length === 0
                    label: qsTr("Подключение GitHub")
                    keywords: "аккаунт синхронизация github вход client id"
                    CText {
                        Layout.fillWidth: true; wrapMode: Text.WordWrap; size: 13.5; color: Theme.onSurfaceVariant
                        text: qsTr("Аккаунт Crumb хранится в вашем GitHub (секретный Gist, зашифрованный вашим паролем). "
                                 + "Для входа нужен Client ID: github.com → Settings → Developer settings → OAuth Apps → "
                                 + "New OAuth App, включите «Enable Device Flow» и вставьте Client ID сюда.")
                    }
                }
                SField {
                    visible: shown && !Sync.configured
                    label: qsTr("Client ID приложения GitHub")
                    keywords: "github oauth client id"
                    placeholder: "Ov23li…"
                    text: Settings.syncClientId
                    onEdited: (t) => Settings.syncClientId = t.trim()
                }

                // Не вошли
                SAction {
                    visible: shown && Sync.state === "signedOut"
                    label: qsTr("Войти через GitHub")
                    hint: qsTr("Настройки, тема, домашняя страница, быстрые сайты, расширения и подписки Rewind — на всех ваших компьютерах")
                    keywords: "аккаунт вход синхронизация github"
                    icon: "\uE77B"; buttonText: Sync.busy ? qsTr("Подождите…") : qsTr("Войти"); variant: "filled"
                    onTriggered: if (!Sync.busy) Sync.signIn()
                }

                // Ждём подтверждения на github.com
                Row_ {
                    visible: shown && Sync.state === "waitingCode"
                    label: qsTr("Код входа")
                    keywords: "аккаунт вход код github"
                    CText {
                        Layout.fillWidth: true; wrapMode: Text.WordWrap; size: 13.5; color: Theme.onSurfaceVariant
                        text: qsTr("Откройте %1, введите этот код и нажмите «Authorize». Crumb подхватит вход сам.").arg(Sync.verifyUrl)
                    }
                    RowLayout {
                        spacing: 12
                        Rectangle {
                            implicitWidth: codeText.implicitWidth + 40; implicitHeight: 58; radius: 20
                            color: Theme.primaryContainer
                            CText {
                                id: codeText; anchors.centerIn: parent
                                text: Sync.userCode; size: 30; font.weight: Font.Bold; font.letterSpacing: 4
                                color: Theme.onPrimaryContainer
                            }
                        }
                        CookieButton {
                            icon: "\uE8C8"; text: qsTr("Скопировать"); variant: "tonal"
                            onClicked: { clip.text = Sync.userCode; clip.selectAll(); clip.copy() }
                        }
                        CookieButton {
                            icon: "\uE8A7"; text: qsTr("Открыть GitHub"); variant: "filled"
                            onClicked: CefRuntime.openUrlRequest(Sync.verifyUrl)
                        }
                        CookieButton { icon: "\uE711"; variant: "surface"; toolTip: qsTr("Отмена"); onClicked: Sync.cancelSignIn() }
                        TextEdit { id: clip; visible: false }
                    }
                }

                // Пароль синхронизации
                Row_ {
                    id: passRow
                    // reset — «Забыли пароль?»: задать новый, облако перезапишется данными этого ПК
                    property bool reset: false
                    function submit() {
                        const ok = reset ? Sync.resetPassphrase(passInput.text) : Sync.setPassphrase(passInput.text)
                        if (ok) { passInput.text = ""; reset = false }
                    }
                    visible: shown && Sync.state === "needPassphrase"
                    label: reset ? qsTr("Новый пароль синхронизации")
                         : Sync.remoteExists ? qsTr("Введите пароль синхронизации") : qsTr("Придумайте пароль синхронизации")
                    keywords: "пароль синхронизации шифрование"
                    CText {
                        Layout.fillWidth: true; wrapMode: Text.WordWrap; size: 13.5; color: Theme.onSurfaceVariant
                        text: passRow.reset
                              ? qsTr("Старый пароль восстановить нельзя: им зашифрованы данные в облаке, и прочитать их без него не может никто. "
                                   + "Задайте новый пароль — данные в облаке заменятся настройками, домашней страницей и расширениями с этого компьютера. "
                                   + "На других компьютерах Crumb попросит ввести новый пароль.")
                              : Sync.remoteExists
                              ? qsTr("Тот же пароль, что вы задали на первом компьютере. Без него данные из облака не прочитать.")
                              : qsTr("Им шифруются ваши данные перед отправкой в GitHub. Он понадобится на других компьютерах. "
                                   + "Если забудете — данные в облаке не восстановить (настройки на этом компьютере останутся).")
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Rectangle {
                            Layout.fillWidth: true; implicitHeight: 44; radius: 22
                            color: Theme.surfaceContainerHighest
                            border.width: passInput.activeFocus ? 2 : 0; border.color: Theme.primary
                            TextInput {
                                id: passInput
                                anchors.fill: parent; anchors.leftMargin: 18; anchors.rightMargin: 18
                                verticalAlignment: TextInput.AlignVCenter
                                echoMode: TextInput.Password
                                font.pixelSize: Prefs.px(15); color: Theme.onSurface
                                onAccepted: passRow.submit()
                                CText {
                                    anchors.fill: parent; verticalAlignment: Text.AlignVCenter
                                    visible: passInput.text.length === 0
                                    text: qsTr("Не короче 6 символов"); color: Theme.onSurfaceVariant
                                }
                            }
                        }
                        CookieButton {
                            icon: passRow.reset ? "\uE72C" : "\uE73E"
                            text: passRow.reset ? qsTr("Сбросить и сохранить") : qsTr("Готово")
                            variant: "filled"
                            onClicked: passRow.submit()
                        }
                    }
                    CText {
                        visible: Sync.remoteExists
                        text: passRow.reset ? qsTr("Я вспомнил пароль") : qsTr("Забыли пароль?")
                        size: 13.5; color: Theme.primary; font.underline: forgotHover.hovered
                        HoverHandler { id: forgotHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: { passRow.reset = !passRow.reset; passInput.text = ""; passInput.forceActiveFocus() } }
                    }
                }

                // Сменить пароль синхронизации (когда всё работает)
                Row_ {
                    id: changeRow
                    property bool open: false
                    function save() {
                        if (!Sync.resetPassphrase(newPassInput.text)) return
                        newPassInput.text = ""
                        open = false
                        toast.show(qsTr("Пароль синхронизации изменён"))
                    }
                    visible: shown && Sync.state === "ready"
                    label: qsTr("Пароль синхронизации")
                    keywords: "сменить пароль синхронизации забыл сбросить"
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        CText {
                            Layout.fillWidth: true; wrapMode: Text.WordWrap; size: 13.5; color: Theme.onSurfaceVariant
                            text: changeRow.open ? qsTr("Данные в облаке перешифруются новым паролем. На других компьютерах Crumb попросит ввести его.")
                                                 : qsTr("Им зашифрованы ваши данные в GitHub. Забыли — задайте новый здесь")
                        }
                        CookieButton {
                            visible: !changeRow.open
                            icon: "\uE70F"; text: qsTr("Сменить"); variant: "tonal"
                            onClicked: { changeRow.open = true; newPassInput.forceActiveFocus() }
                        }
                    }
                    RowLayout {
                        visible: changeRow.open
                        Layout.fillWidth: true
                        spacing: 10
                        Rectangle {
                            Layout.fillWidth: true; implicitHeight: 44; radius: 22
                            color: Theme.surfaceContainerHighest
                            border.width: newPassInput.activeFocus ? 2 : 0; border.color: Theme.primary
                            TextInput {
                                id: newPassInput
                                anchors.fill: parent; anchors.leftMargin: 18; anchors.rightMargin: 18
                                verticalAlignment: TextInput.AlignVCenter
                                echoMode: TextInput.Password
                                font.pixelSize: Prefs.px(15); color: Theme.onSurface
                                onAccepted: changeRow.save()
                                CText {
                                    anchors.fill: parent; verticalAlignment: Text.AlignVCenter
                                    visible: newPassInput.text.length === 0
                                    text: qsTr("Новый пароль, не короче 6 символов"); color: Theme.onSurfaceVariant
                                }
                            }
                        }
                        CookieButton { icon: "\uE711"; variant: "surface"; toolTip: qsTr("Отмена"); onClicked: { newPassInput.text = ""; changeRow.open = false } }
                        CookieButton { icon: "\uE73E"; text: qsTr("Сохранить"); variant: "filled"; onClicked: changeRow.save() }
                    }
                }

                // Вошли
                Row_ {
                    visible: shown && (Sync.state === "ready" || Sync.state === "needPassphrase") && Sync.login.length > 0
                    label: qsTr("Вы вошли")
                    keywords: "аккаунт профиль github синхронизация выйти"
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 14
                        Item {
                            width: 60; height: 60
                            Image {
                                id: avatarImg
                                anchors.fill: parent
                                source: Sync.avatar; fillMode: Image.PreserveAspectCrop
                                sourceSize: Qt.size(120, 120); asynchronous: true
                                visible: false
                            }
                            Item {
                                id: avatarMask
                                anchors.fill: parent
                                layer.enabled: true
                                visible: false
                                CookieShape { anchors.fill: parent; color: "black" }
                            }
                            CookieShape { anchors.fill: parent; color: Theme.primaryContainer; visible: avatarImg.status !== Image.Ready }
                            MultiEffect {
                                anchors.fill: parent
                                source: avatarImg
                                maskEnabled: true
                                maskSource: avatarMask
                                visible: avatarImg.status === Image.Ready
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            CText { text: Sync.name; size: 17; font.weight: Font.DemiBold }
                            CText { text: "@" + Sync.login + " · GitHub"; size: 13; color: Theme.onSurfaceVariant }
                            CText {
                                size: 12.5; color: Sync.error.length ? Theme.error : Theme.primary
                                Layout.fillWidth: true; wrapMode: Text.WordWrap
                                text: Sync.error.length ? Sync.error
                                      : Sync.busy ? qsTr("Синхронизация…")
                                      : Sync.lastSync.length ? qsTr("Синхронизировано %1").arg(Sync.lastSync)
                                      : qsTr("Ещё не синхронизировано")
                            }
                        }
                        CookieButton {
                            visible: Sync.state === "ready"
                            icon: "\uE895"; text: qsTr("Сейчас"); variant: "tonal"
                            toolTip: qsTr("Синхронизировать сейчас")
                            onClicked: Sync.syncNow()
                        }
                        CookieButton {
                            icon: "\uF3B1"; text: qsTr("Выйти"); variant: "surface"
                            onClicked: Sync.signOut(false)
                        }
                    }
                }
                CText {
                    visible: Sync.error.length > 0 && Sync.login.length === 0
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    text: Sync.error; color: Theme.error; size: 13
                }

                // Расширения из аккаунта, которых нет на этом компьютере
                Row_ {
                    visible: shown && Sync.state === "ready" && Sync.missingExtensions.length > 0
                    label: qsTr("Расширения с других компьютеров")
                    hint: qsTr("Установите из Интернет-магазина Chrome — одним нажатием «Установить» на его странице")
                    keywords: "расширения синхронизация установить"
                    Repeater {
                        model: Sync.missingExtensions
                        RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: 10
                            CookieShape {
                                width: 34; height: 34; color: Theme.surfaceContainerHighest
                                CText { anchors.centerIn: parent; text: (modelData.name || "?")[0]; size: 15; font.weight: Font.DemiBold }
                            }
                            CText { text: modelData.name || modelData.id; size: 14.5; Layout.fillWidth: true; elide: Text.ElideRight }
                            CookieButton {
                                icon: "\uE896"; text: qsTr("Установить"); variant: "filled"; size: 32
                                onClicked: CefRuntime.openUrlRequest("https://chromewebstore.google.com/detail/" + modelData.id)
                            }
                            CookieButton {
                                icon: "\uE711"; variant: "surface"; size: 32
                                toolTip: qsTr("Убрать из аккаунта")
                                onClicked: Sync.dismissExtension(modelData.id)
                            }
                        }
                    }
                }
                SAction {
                    visible: shown && Sync.state === "ready"
                    label: qsTr("Выйти и удалить данные из облака")
                    hint: qsTr("Удалит Gist с вашими зашифрованными данными. Настройки на этом компьютере останутся")
                    keywords: "удалить аккаунт облако"
                    icon: "\uE74D"; buttonText: qsTr("Удалить"); buttonColor: Theme.error
                    onTriggered: Sync.signOut(true)
                }
            }

            // ------------------------------------------------------ Цвета
            Section {
                title: qsTr("Цвета"); icon: ""
                SChoice {
                    label: qsTr("Тема")
                    keywords: "светлая тёмная тёмный режим расписание"
                    options: [{ value: "system", label: qsTr("Как в системе"), icon: "" },
                              { value: "light", label: qsTr("Светлая"), icon: "" },
                              { value: "dark", label: qsTr("Тёмная"), icon: "" },
                              { value: "schedule", label: qsTr("По расписанию"), icon: "" }]
                    current: Theme.themeMode
                    onChosen: (v) => Theme.themeMode = v
                }
                RowLayout {
                    readonly property bool shown: Theme.themeMode === "schedule" && page.matches("расписание тёмной темы")
                    visible: shown
                    Layout.fillWidth: true
                    spacing: 12
                    SField { label: qsTr("Тёмная тема с"); text: Theme.darkFrom; placeholder: "21:00"; onEdited: (t) => Theme.darkFrom = t }
                    SField { label: qsTr("до"); text: Theme.darkTo; placeholder: "07:00"; onEdited: (t) => Theme.darkTo = t }
                }
                SChoice {
                    label: qsTr("Откуда брать цвет")
                    keywords: "обои палитра динамические цвета"
                    options: [{ value: "wallpaper", label: qsTr("Из обоев"), icon: "" },
                              { value: "custom", label: qsTr("Свой цвет"), icon: "" }]
                    current: Theme.sourceType
                    onChosen: (v) => Theme.sourceType = v
                }
                Row_ {
                    label: qsTr("Цвета из обоев")
                    CText { text: parent.label; size: 15 }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 10
                        Repeater {
                            model: Theme.wallpaperColors
                            Swatch {
                                required property color modelData
                                required property int index
                                swatch: modelData
                                selected: Theme.sourceType === "wallpaper" && index === 0
                                onClicked: index === 0 ? Theme.sourceType = "wallpaper" : Theme.customColor = modelData
                            }
                        }
                    }
                }
                Row_ {
                    label: qsTr("Готовые цвета")
                    keywords: "пресеты"
                    CText { text: parent.label; size: 15 }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 10
                        Repeater {
                            model: Theme.presetColors
                            Swatch {
                                required property color modelData
                                swatch: modelData
                                selected: Theme.sourceType === "custom" && Qt.colorEqual(Theme.customColor, modelData)
                                onClicked: Theme.customColor = modelData
                            }
                        }
                    }
                }
                Row_ {
                    id: custom
                    label: qsTr("Свой цвет")
                    keywords: "палитра оттенок насыщенность пипетка"
                    property real hue: Theme.hueOf(Theme.customColor)
                    property real chroma: Math.max(16, Math.min(90, Theme.chromaOf(Theme.customColor)))
                    CText { text: custom.label; size: 15 }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16
                        CookieShape { width: 56; height: 56; color: Theme.customColor }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1   // ширину задаёт только раскладка — без петли пересчёта
                            CText { text: qsTr("Оттенок"); size: 13; color: Theme.onSurfaceVariant }
                            // Радуга под ползунком оттенка
                            Item {
                                Layout.fillWidth: true
                                implicitHeight: 36
                                Rectangle {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width; height: 10; radius: 5
                                    gradient: Gradient {
                                        orientation: Gradient.Horizontal
                                        GradientStop { position: 0.00; color: Theme.toneOf(0, 60, 55) }
                                        GradientStop { position: 0.17; color: Theme.toneOf(60, 60, 55) }
                                        GradientStop { position: 0.33; color: Theme.toneOf(120, 60, 55) }
                                        GradientStop { position: 0.50; color: Theme.toneOf(180, 60, 55) }
                                        GradientStop { position: 0.67; color: Theme.toneOf(240, 60, 55) }
                                        GradientStop { position: 0.83; color: Theme.toneOf(300, 60, 55) }
                                        GradientStop { position: 1.00; color: Theme.toneOf(359, 60, 55) }
                                    }
                                }
                                CookieSlider {
                                    anchors.fill: parent
                                    showTrack: false
                                    from: 0; to: 359; stepSize: 1; value: custom.hue
                                    suffix: "°"
                                    onMoved: (v) => Theme.customColor = Theme.toneOf(v, custom.chroma, 50)
                                }
                            }
                            CText { text: qsTr("Насыщенность"); size: 13; color: Theme.onSurfaceVariant }
                            CookieSlider {
                                Layout.fillWidth: true
                                from: 16; to: 90; stepSize: 1; value: custom.chroma
                                onMoved: (v) => Theme.customColor = Theme.toneOf(custom.hue, v, 50)
                            }
                        }
                        CookieButton {
                            icon: ""
                            text: qsTr("Пипетка")
                            labelPosition: "below"
                            toolTip: qsTr("Взять цвет с экрана")
                            onClicked: page.pickColorFromScreen()
                        }
                    }
                }
                SSlider {
                    label: qsTr("Контрастность")
                    hint: qsTr("Насколько текст и значки отличаются от фона")
                    from: -1; to: 1; value: Theme.contrast
                    formatValue: (v) => (v > 0 ? "+" : "") + Math.round(v * 100) + "%"
                    onMoved: (v) => Theme.contrast = v
                }
                SSwitch {
                    label: qsTr("Чёрная тема для AMOLED")
                    hint: qsTr("Чисто-чёрный фон в тёмной теме — экономит заряд на OLED-экранах")
                    checked: Theme.amoled
                    onToggled: (on) => Theme.amoled = on
                }
                SSlider {
                    label: qsTr("Плавность смены темы")
                    from: 0; to: 1500; stepSize: 50; value: Theme.transitionMs
                    suffix: qsTr(" мс")
                    onMoved: (v) => Theme.transitionMs = v
                }
            }

            // --------------------------------------------------- Печеньки
            Section {
                title: qsTr("Печеньки"); icon: ""
                Row_ {
                    label: qsTr("Предпросмотр")
                    keywords: "форма кнопки"
                    Flow {
                        Layout.fillWidth: true
                        spacing: 18
                        CookieButton { icon: ""; variant: "tonal" }
                        CookieButton { icon: ""; variant: "filled" }
                        CookieButton { icon: ""; variant: "outlined" }
                        CookieButton { icon: ""; variant: "surface"; text: qsTr("Наведите") }
                    }
                }
                SSlider {
                    label: qsTr("Количество волн")
                    from: 6; to: 16; stepSize: 1; value: Prefs.cookieWaves
                    onMoved: (v) => Prefs.cookieWaves = v
                }
                SSlider {
                    label: qsTr("Глубина волн")
                    from: 0.02; to: 0.25; value: Prefs.cookieDepth
                    formatValue: (v) => Math.round(v * 100) + "%"
                    onMoved: (v) => Prefs.cookieDepth = v
                }
                SSlider {
                    label: qsTr("Размер кнопок")
                    from: 0.8; to: 1.4; value: Prefs.cookieScale
                    formatValue: (v) => Math.round(v * 100) + "%"
                    onMoved: (v) => Prefs.cookieScale = v
                }
                SSlider {
                    label: qsTr("Скорость вращения при наведении")
                    from: 0; to: 180; stepSize: 5; value: Prefs.cookieSpin
                    suffix: qsTr("°/с")
                    onMoved: (v) => Prefs.cookieSpin = v
                }
                SSwitch {
                    label: qsTr("«Дыхание» при нажатии")
                    hint: qsTr("Волны вдыхают, когда печеньку нажимают")
                    checked: Prefs.cookieBreathe
                    onToggled: (on) => Prefs.cookieBreathe = on
                }
            }

            // ---------------------------------------------------- Вкладки
            Section {
                title: qsTr("Вкладки"); icon: ""
                SChoice {
                    label: qsTr("При запуске")
                    hint: qsTr("Восстановленные вкладки загружаются, только когда вы их открываете")
                    keywords: "восстановить вкладки сеанс запуск"
                    options: [{ value: "restore", label: qsTr("Вернуть вкладки"), icon: "\uE7A7" },
                              { value: "home", label: qsTr("Новая вкладка"), icon: "\uE80F" }]
                    current: Settings.startupMode
                    onChosen: (v) => Settings.startupMode = v
                }
                SChoice {
                    label: qsTr("Расположение")
                    keywords: "вертикальные боковые"
                    options: [{ value: "top", label: qsTr("Сверху"), icon: "" },
                              { value: "side", label: qsTr("Сбоку"), icon: "" }]
                    current: Settings.tabsPosition
                    onChosen: (v) => Settings.tabsPosition = v
                }
                SSlider {
                    label: qsTr("Высота вкладок")
                    from: 32; to: 52; stepSize: 1; value: Settings.tabsHeight
                    suffix: " px"
                    onMoved: (v) => Settings.tabsHeight = v
                }
                SSlider {
                    label: qsTr("Скругление")
                    from: 0; to: 20; stepSize: 1; value: Settings.tabsRadius
                    suffix: " px"
                    onMoved: (v) => Settings.tabsRadius = v
                }
                SSwitch {
                    label: qsTr("Показывать значки сайтов")
                    checked: Settings.tabsIcons
                    onToggled: (on) => Settings.tabsIcons = on
                }
                SSwitch {
                    label: qsTr("Группы вкладок")
                    hint: qsTr("Пункты «Добавить в группу» в меню вкладки")
                    checked: Settings.tabsGroups
                    onToggled: (on) => Settings.tabsGroups = on
                }
            }

            // ------------------------------------------------------ Панель
            Section {
                title: qsTr("Панель"); icon: ""
                Row_ {
                    id: toolbarRow
                    label: qsTr("Кнопки на панели")
                    hint: qsTr("Включайте нужные и перетаскивайте за ручку, чтобы поменять порядок")
                    keywords: "порядок перетаскивание"
                    readonly property var all: [
                        { id: "back", label: qsTr("Назад"), icon: "" },
                        { id: "forward", label: qsTr("Вперёд"), icon: "" },
                        { id: "reload", label: qsTr("Обновить"), icon: "" },
                        { id: "home", label: qsTr("Домой"), icon: "" },
                        { id: "newTab", label: qsTr("Новая вкладка"), icon: "\uE710" },
                        { id: "extensions", label: qsTr("Расширения («банка с печеньем»)"), icon: "\uEA86" },
                        { id: "rewind", label: qsTr("Rewind — машина времени"), icon: "\uE81C" },
                        { id: "history", label: qsTr("История"), icon: "\uE823" },
                        { id: "passwords", label: qsTr("Пароли"), icon: "\uE8D7" },
                        { id: "downloads", label: qsTr("Загрузки"), icon: "\uE896" },
                        { id: "theme", label: qsTr("Тема"), icon: "" },
                        { id: "settings", label: qsTr("Настройки"), icon: "" }
                    ]
                    // Порядок: сначала включённые (в их порядке), затем выключенные.
                    readonly property var ordered: {
                        const on = Settings.toolbarButtons || []
                        const res = []
                        for (const id of on) { const b = all.find(x => x.id === id); if (b) res.push(b) }
                        for (const b of all) if (on.indexOf(b.id) < 0) res.push(b)
                        return res
                    }
                    function setEnabled(id, enabled) {
                        const on = (Settings.toolbarButtons || []).slice()
                        const i = on.indexOf(id)
                        if (enabled && i < 0) on.push(id)
                        if (!enabled && i >= 0) on.splice(i, 1)
                        Settings.toolbarButtons = on
                    }
                    function move(from, to) {
                        const ids = ordered.map(b => b.id)
                        const [m] = ids.splice(from, 1)
                        ids.splice(to, 0, m)
                        const on = Settings.toolbarButtons || []
                        Settings.toolbarButtons = ids.filter(id => on.indexOf(id) >= 0)
                    }

                    CText { text: toolbarRow.label; size: 15 }
                    CText { text: toolbarRow.hint; size: 12.5; color: Theme.onSurfaceVariant; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    Item {
                        id: tbList
                        Layout.fillWidth: true
                        implicitHeight: toolbarRow.ordered.length * 52
                        Repeater {
                            model: toolbarRow.ordered
                            Rectangle {
                                id: tbItem
                                required property var modelData
                                required property int index
                                readonly property bool on: (Settings.toolbarButtons || []).indexOf(modelData.id) >= 0
                                width: tbList.width
                                height: 48
                                y: dragH.active ? y : index * 52
                                z: dragH.active ? 2 : 1
                                radius: 16
                                color: dragH.active ? Theme.surfaceContainerHighest : Theme.surfaceContainer
                                Behavior on y { enabled: !dragH.active && Prefs.animations; SpringAnimation { spring: Prefs.spring(4); damping: 0.3 } }
                                Text {   // ручка
                                    id: grip
                                    x: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: ""
                                    font.family: Theme.iconFont
                                    font.pixelSize: 16
                                    color: Theme.onSurfaceVariant
                                    HoverHandler { cursorShape: Qt.SizeVerCursor }
                                    DragHandler {
                                        id: dragH
                                        target: tbItem
                                        xAxis.enabled: false
                                        onActiveChanged: if (!active) {
                                            const to = Math.max(0, Math.min(toolbarRow.ordered.length - 1, Math.round(tbItem.y / 52)))
                                            const from = tbItem.index
                                            tbItem.y = Qt.binding(() => tbItem.index * 52)
                                            if (to !== from) toolbarRow.move(from, to)
                                        }
                                    }
                                }
                                CookieShape {
                                    x: 40
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 30; height: 30
                                    color: tbItem.on ? Theme.secondaryContainer : Theme.surfaceContainerHighest
                                    Text {
                                        anchors.centerIn: parent
                                        text: tbItem.modelData.icon
                                        font.family: Theme.iconFont
                                        font.pixelSize: 13
                                        color: Theme.onSecondaryContainer
                                    }
                                }
                                CookieSwitch {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 84
                                    anchors.right: parent.right
                                    anchors.rightMargin: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: tbItem.modelData.label
                                    checked: tbItem.on
                                    onToggled: (v) => toolbarRow.setEnabled(tbItem.modelData.id, v)
                                }
                            }
                        }
                    }
                }
                SSwitch {
                    label: qsTr("Компактный режим")
                    hint: qsTr("Панель ниже, кнопки меньше")
                    checked: Settings.toolbarCompact
                    onToggled: (on) => Settings.toolbarCompact = on
                }
            }

            // ------------------------------------------------------ Шрифты
            Section {
                title: qsTr("Шрифты"); icon: ""
                Row_ {
                    id: fontRow
                    label: qsTr("Шрифт интерфейса")
                    keywords: "гарнитура"
                    readonly property var candidates: ["", "Segoe UI Variable Display", "Segoe UI", "Arial", "Verdana",
                        "Tahoma", "Trebuchet MS", "Calibri", "Candara", "Georgia", "Cambria", "Consolas", "Comic Sans MS"]
                    readonly property var available: candidates.filter(f => f === "" || Qt.fontFamilies().indexOf(f) >= 0)
                    RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            CText { text: fontRow.label; size: 15 }
                            CText {
                                text: qsTr("Сейчас: %1 — Съешь же ещё этих мягких булок").arg(Prefs.font)
                                size: 13
                                color: Theme.onSurfaceVariant
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }
                        CookieButton {
                            icon: ""
                            text: qsTr("Выбрать")
                            onClicked: fontMenu.popup()
                            ThemedMenu {
                                id: fontMenu
                                Repeater {
                                    model: fontRow.available
                                    ThemedMenuItem {
                                        required property string modelData
                                        text: modelData === "" ? qsTr("Roboto Flex (по умолчанию)") : modelData
                                        checked: Prefs.fontFamily === modelData
                                        fontFamily: modelData === "" ? Theme.fontFamily : modelData
                                        onTriggered: Prefs.fontFamily = modelData
                                    }
                                }
                            }
                        }
                    }
                }
                SSlider {
                    label: qsTr("Размер текста интерфейса")
                    from: 0.85; to: 1.3; value: Prefs.textScale
                    formatValue: (v) => Math.round(v * 100) + "%"
                    onMoved: (v) => Prefs.textScale = v
                }
                SSlider {
                    label: qsTr("Размер текста на сайтах")
                    hint: qsTr("Шрифт по умолчанию для страниц, где размер не задан самим сайтом")
                    from: 12; to: 24; stepSize: 1; value: Settings.pagesFontSize
                    suffix: " px"
                    onMoved: (v) => Settings.pagesFontSize = v
                }
            }

            // ---------------------------------------------------- Анимации
            Section {
                title: qsTr("Анимации"); icon: ""
                SSwitch {
                    label: qsTr("Анимации")
                    hint: qsTr("Выключите, если нужно максимально спокойное поведение интерфейса")
                    keywords: "отключить движение"
                    checked: Prefs.animations
                    onToggled: (on) => Prefs.animations = on
                }
                SSlider {
                    label: qsTr("Скорость")
                    from: 0.25; to: 3; value: Prefs.animSpeed
                    formatValue: (v) => "×" + v.toFixed(2)
                    enabled: Prefs.animations
                    onMoved: (v) => Prefs.animSpeed = v
                }
                SSlider {
                    label: qsTr("Сила пружины")
                    hint: qsTr("Чем сильнее, тем резче и «упруже» отскок")
                    from: 0.4; to: 2.5; value: Prefs.springStrength
                    formatValue: (v) => "×" + v.toFixed(2)
                    enabled: Prefs.animations
                    onMoved: (v) => Prefs.springStrength = v
                }
            }

            // ------------------------------------------- Домашняя страница
            Section {
                title: qsTr("Домашняя страница"); icon: ""
                SSwitch { label: qsTr("Часы и дата"); checked: Settings.homeShowClock; onToggled: (on) => Settings.homeShowClock = on }
                SSwitch { label: qsTr("Приветствие"); checked: Settings.homeShowGreeting; onToggled: (on) => Settings.homeShowGreeting = on }
                SSwitch { label: qsTr("Строка поиска"); checked: Settings.homeShowSearch; onToggled: (on) => Settings.homeShowSearch = on }
                SSwitch { label: qsTr("Быстрые сайты"); checked: Settings.homeShowSites; onToggled: (on) => Settings.homeShowSites = on }
                SSwitch { label: qsTr("Погода"); checked: Settings.homeShowWeather; onToggled: (on) => Settings.homeShowWeather = on }
                SSwitch { label: qsTr("Заметки"); checked: Settings.homeShowNotes; onToggled: (on) => Settings.homeShowNotes = on }
                SChoice {
                    label: qsTr("Фон")
                    keywords: "обои картинка градиент"
                    options: [{ value: "gradient", label: qsTr("Градиент"), icon: "" },
                              { value: "solid", label: qsTr("Цвет темы"), icon: "" },
                              { value: "wallpaper", label: qsTr("Обои"), icon: "" },
                              { value: "image", label: qsTr("Картинка"), icon: "" }]
                    current: Settings.homeBackground
                    onChosen: (v) => v === "image" && !Settings.homeImage.length ? homeImageDialog.open() : Settings.homeBackground = v
                }
                Row_ {
                    label: qsTr("Градиентный фон")
                    keywords: "обои градиент рассвет океан аврора закат космос сакура"
                    CText { text: parent.label; size: 15 }
                    Flow {
                        Layout.fillWidth: true
                        Layout.topMargin: 6
                        spacing: 14
                        Repeater {
                            model: Gradients.list
                            CookieButton {
                                required property var modelData
                                readonly property bool selected: Settings.homeBackground === "gradient" && Settings.homeGradient === modelData.id
                                size: 44
                                labelPosition: "below"
                                labelWidth: 66
                                text: modelData.name
                                gradientColors: modelData.colors.length ? modelData.colors
                                                : [Theme.primaryContainer, Theme.tertiaryContainer, Theme.secondaryContainer]
                                contentColor: "white"
                                icon: selected ? "" : ""
                                depth: selected ? Prefs.cookieDepth * 2 : Prefs.cookieDepth
                                onClicked: { Settings.homeGradient = modelData.id; Settings.homeBackground = "gradient" }
                            }
                        }
                    }
                }
                SAction {
                    label: qsTr("Своя картинка")
                    hint: Settings.homeImage.length ? Settings.homeImage : qsTr("Не выбрана")
                    icon: ""; buttonText: qsTr("Выбрать…")
                    onTriggered: homeImageDialog.open()
                }
                SSlider { label: qsTr("Размытие фона"); from: 0; to: 60; stepSize: 1; value: Settings.homeBlur; onMoved: (v) => Settings.homeBlur = v }
                SSlider {
                    label: qsTr("Затемнение картинки")
                    from: 0; to: 0.7; value: Settings.homeDim
                    formatValue: (v) => Math.round(v * 100) + "%"
                    onMoved: (v) => Settings.homeDim = v
                }
                SSlider { label: qsTr("Колонок в сетке сайтов"); from: 3; to: 10; stepSize: 1; value: Settings.homeColumns; onMoved: (v) => Settings.homeColumns = v }
                SSlider { label: qsTr("Размер значков сайтов"); from: 44; to: 96; stepSize: 2; suffix: " px"; value: Settings.homeTileSize; onMoved: (v) => Settings.homeTileSize = v }
                SField {
                    label: qsTr("Город для погоды")
                    hint: qsTr("Любой город или село, опечатки поправляются. Данные Open-Meteo и OpenStreetMap; местоположение не определяется автоматически")
                    text: Settings.homeCity
                    placeholder: qsTr("Например, Алматы")
                    onEdited: (t) => Settings.homeCity = t.trim()
                }
            }

            // --------------------------------------------- Курсор и звуки
            Section {
                title: qsTr("Курсор и звуки"); icon: ""
                SSwitch {
                    label: qsTr("Курсор-«рука» над печеньками")
                    checked: Prefs.handCursor
                    onToggled: (on) => Prefs.handCursor = on
                }
                SSwitch {
                    label: qsTr("Звук нажатия на печеньки")
                    hint: qsTr("Мягкий «чпок» при каждом нажатии")
                    checked: Prefs.clickSounds
                    onToggled: (on) => Prefs.clickSounds = on
                }
                SSlider {
                    label: qsTr("Громкость")
                    from: 0; to: 1; value: Prefs.clickVolume
                    enabled: Prefs.clickSounds
                    formatValue: (v) => Math.round(v * 100) + "%"
                    onMoved: (v) => Prefs.clickVolume = v
                }
                SAction {
                    label: qsTr("Проверить звук")
                    icon: ""; buttonText: qsTr("Чпок")
                    enabled: Prefs.clickSounds
                    onTriggered: {}
                }
            }

            // ------------------------------------------------------ Rewind
            Section {
                title: qsTr("Rewind"); icon: ""
                Row_ {
                    label: qsTr("О функции")
                    CText {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        size: 13.5
                        color: Theme.onSurfaceVariant
                        text: qsTr("Rewind — машина времени для страниц: Crumb будет сохранять снимки страниц на этом компьютере. "
                                 + "Открыть прошлую версию, увидеть, что изменилось, следить за фрагментом и искать по всему, что вы видели — "
                                 + "кнопка Rewind на панели или %1.").arg(Settings.shortcutRewind)
                    }
                }
                SSwitch {
                    label: qsTr("Сохранять снимки страниц")
                    checked: Settings.rewindEnabled
                    onToggled: (on) => Settings.rewindEnabled = on
                }
                SSlider {
                    label: qsTr("Лимит места на диске")
                    from: 256; to: 20480; stepSize: 256; value: Settings.rewindLimitMB
                    formatValue: (v) => v >= 1024 ? (v / 1024).toFixed(1) + qsTr(" ГБ") : Math.round(v) + qsTr(" МБ")
                    onMoved: (v) => Settings.rewindLimitMB = v
                }
                SSlider {
                    label: qsTr("Хранить снимки")
                    from: 1; to: 365; stepSize: 1; value: Settings.rewindDays
                    formatValue: (v) => Math.round(v) + qsTr(" дн.")
                    onMoved: (v) => Settings.rewindDays = v
                }
                SField {
                    label: qsTr("Чёрный список сайтов")
                    hint: qsTr("По одному на строку, можно со звёздочкой: *bank*. Приватные вкладки не сохраняются никогда")
                    multiline: true
                    text: Settings.rewindBlacklist
                    onEdited: (t) => Settings.rewindBlacklist = t
                }
                SAction {
                    label: qsTr("Удалить все снимки")
                    hint: qsTr("Сейчас: %1 снимков, %2 МБ. Подписки на изменения останутся").arg(Rewind.count).arg(Rewind.sizeMB.toFixed(1))
                    keywords: "очистить rewind снимки"
                    icon: "\uE74D"; buttonText: qsTr("Удалить")
                    onTriggered: Rewind.clearAll()
                }
            }

            // ------------------------------------------- История и пароли
            Section {
                title: qsTr("История и пароли"); icon: ""
                SSwitch {
                    label: qsTr("Сохранять историю посещений")
                    hint: qsTr("Нужна для страницы «История» и подсказок в адресной строке. Хранится полгода")
                    keywords: "история посещений подсказки"
                    checked: Settings.historyEnabled
                    onToggled: (on) => Settings.historyEnabled = on
                }
                SAction {
                    label: qsTr("История")
                    hint: qsTr("Посещений: %1 · %2").arg(History.count).arg(Settings.shortcutHistory)
                    icon: ""; buttonText: qsTr("Открыть")
                    onTriggered: CefRuntime.openUrlRequest("crumb://history")
                }
                SAction {
                    label: qsTr("Удалить всю историю")
                    keywords: "очистить история"
                    icon: ""; buttonText: qsTr("Удалить")
                    onTriggered: { History.clear(0); toast.show(qsTr("История удалена")) }
                }
                SSwitch {
                    label: qsTr("Предлагать сохранить пароли")
                    hint: qsTr("После входа на сайт Crumb спросит, запомнить ли пароль. Пароли шифруются Windows")
                    keywords: "пароли менеджер сохранить"
                    checked: Settings.passwordsOffer
                    onToggled: (on) => Settings.passwordsOffer = on
                }
                SSwitch {
                    label: qsTr("Подставлять сохранённые пароли")
                    hint: qsTr("Нажмите в поле логина или пароля — появится список ваших входов на этом сайте")
                    keywords: "автозаполнение пароли"
                    checked: Settings.passwordsAutofill
                    onToggled: (on) => Settings.passwordsAutofill = on
                }
                SAction {
                    label: qsTr("Менеджер паролей")
                    hint: qsTr("Сохранено: %1. Показать пароль можно после Windows Hello").arg(Passwords.count)
                    keywords: "пароли"
                    icon: ""; buttonText: qsTr("Открыть")
                    onTriggered: CefRuntime.openUrlRequest("crumb://passwords")
                }
            }

            // -------------------------------------------------- Приватность
            Section {
                title: qsTr("Приватность"); icon: ""
                SSwitch {
                    label: qsTr("Просить сайты не отслеживать меня")
                    hint: qsTr("Отправлять заголовок Do Not Track")
                    checked: Settings.privacyDnt
                    onToggled: (on) => Settings.privacyDnt = on
                }
                SSwitch {
                    label: qsTr("Блокировать сторонние cookie")
                    hint: qsTr("Некоторые сайты (вход через другие сервисы) могут работать хуже")
                    checked: Settings.privacyBlock3p
                    onToggled: (on) => Settings.privacyBlock3p = on
                }
                SAction {
                    label: qsTr("Удалить cookie и кэш")
                    hint: qsTr("Вы выйдете из аккаунтов на сайтах")
                    icon: ""; buttonText: qsTr("Удалить")
                    onTriggered: { CefRuntime.clearBrowsingData(); toast.show(qsTr("Cookie и кэш удалены")) }
                }
            }

            // ---------------------------------------------------- Поисковик
            Section {
                title: qsTr("Поисковик"); icon: ""
                SChoice {
                    label: qsTr("Искать с помощью")
                    keywords: "google яндекс duckduckgo bing brave ecosia"
                    options: Engines.list.map(e => ({ value: e.id, label: e.name, letter: e.letter, iconSource: e.icon }))
                    current: Settings.searchEngine
                    onChosen: (v) => Settings.searchEngine = v
                }
                SField {
                    visible: shown && Settings.searchEngine === "custom"
                    label: qsTr("Адрес своего поисковика")
                    hint: qsTr("%s заменится на запрос")
                    text: Settings.searchCustom
                    onEdited: (t) => Settings.searchCustom = t
                }
            }

            // ----------------------------------------------------- Загрузки
            Section {
                title: qsTr("Загрузки"); icon: ""
                SAction {
                    label: qsTr("Папка для загрузок")
                    hint: Settings.downloadsDir
                    icon: ""; buttonText: qsTr("Изменить…")
                    onTriggered: downloadsDialog.open()
                }
                SSwitch {
                    label: qsTr("Каждый раз спрашивать, куда сохранить")
                    checked: Settings.downloadsAsk
                    onToggled: (on) => Settings.downloadsAsk = on
                }
            }

            // ---------------------------------------------- Горячие клавиши
            Section {
                title: qsTr("Горячие клавиши"); icon: ""
                Repeater {
                    model: [["shortcutNewTab", qsTr("Новая вкладка")], ["shortcutCloseTab", qsTr("Закрыть вкладку")],
                            ["shortcutReopenTab", qsTr("Вернуть закрытую вкладку")], ["shortcutNextTab", qsTr("Следующая вкладка")],
                            ["shortcutPrevTab", qsTr("Предыдущая вкладка")], ["shortcutAddress", qsTr("Адресная строка")],
                            ["shortcutReload", qsTr("Обновить страницу")], ["shortcutBack", qsTr("Назад")],
                            ["shortcutForward", qsTr("Вперёд")], ["shortcutHome", qsTr("Домашняя страница")],
                            ["shortcutSettings", qsTr("Настройки")], ["shortcutDownloads", qsTr("Загрузки")], ["shortcutRewind", qsTr("Rewind")],
                            ["shortcutHistory", qsTr("История")], ["shortcutFind", qsTr("Поиск на странице")]]
                    Row_ {
                        id: sc
                        required property var modelData
                        label: modelData[1]
                        keywords: Settings[modelData[0]]
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12
                            CText { text: sc.label; size: 15; Layout.fillWidth: true }
                            // Поле записи сочетания: нажмите и затем нужные клавиши
                            Rectangle {
                                id: keyBox
                                Layout.preferredWidth: 190
                                Layout.preferredHeight: 38
                                radius: 19
                                color: keyBox.activeFocus ? Theme.primaryContainer : Theme.surfaceContainerHighest
                                border.width: keyBox.activeFocus ? 2 : 0
                                border.color: Theme.primary
                                activeFocusOnTab: true
                                CText {
                                    anchors.centerIn: parent
                                    text: keyBox.activeFocus ? qsTr("Нажмите сочетание…") : (Settings[sc.modelData[0]] || "—")
                                    size: 13.5
                                    font.weight: Font.Medium
                                    color: keyBox.activeFocus ? Theme.onPrimaryContainer : Theme.onSurface
                                }
                                TapHandler { onTapped: keyBox.forceActiveFocus() }
                                Keys.onPressed: (e) => {
                                    e.accepted = true
                                    if (e.key === Qt.Key_Escape) { keyBox.focus = false; return }
                                    const seq = Settings.keyToString(e.key, e.modifiers)
                                    if (seq.length) { Settings[sc.modelData[0]] = seq; keyBox.focus = false }
                                }
                            }
                            CookieButton {
                                size: 32
                                icon: ""
                                variant: "surface"
                                toolTip: qsTr("Как было: %1").arg(Settings.defaultValue(sc.modelData[0]))
                                onClicked: Settings.reset(sc.modelData[0])
                            }
                        }
                    }
                }
            }

            // ------------------------------------- Резервная копия и сброс
            Section {
                title: qsTr("Резервная копия и сброс"); icon: ""
                SAction {
                    label: qsTr("Экспорт всех настроек")
                    hint: qsTr("Один файл .json: цвета, печеньки, вкладки, домашняя страница, клавиши…")
                    icon: ""; buttonText: qsTr("Сохранить…")
                    onTriggered: exportDialog.open()
                }
                SAction {
                    label: qsTr("Импорт настроек из файла")
                    icon: ""; buttonText: qsTr("Открыть…")
                    onTriggered: importDialog.open()
                }
                SAction {
                    label: qsTr("Сбросить всё")
                    hint: qsTr("Все настройки вернутся к исходным. Закладки и история не пострадают")
                    keywords: "сброс по умолчанию"
                    icon: ""; buttonText: qsTr("Сбросить всё")
                    buttonColor: Theme.error
                    onTriggered: resetPopup.open()
                }
            }

            // ------------------------------------------------------ О Crumb
            Section {
                title: qsTr("О Crumb"); icon: "\uE946"
                SAction {
                    label: DefaultBrowser.isDefault ? qsTr("Crumb — браузер по умолчанию") : qsTr("Браузер по умолчанию")
                    hint: DefaultBrowser.isDefault ? qsTr("Ссылки из других программ открываются в Crumb")
                                                   : qsTr("Сейчас ссылки открывает другой браузер. Windows попросит подтвердить выбор")
                    keywords: "по умолчанию default ссылки"
                    icon: DefaultBrowser.isDefault ? "\uE73E" : "\uE8A7"
                    buttonText: DefaultBrowser.isDefault ? qsTr("Открыть параметры") : qsTr("Сделать")
                    variant: DefaultBrowser.isDefault ? "tonal" : "filled"
                    onTriggered: DefaultBrowser.openSystemSettings()
                }
                SSwitch {
                    label: qsTr("Записывать ошибки страниц в журнал")
                    hint: qsTr("Предупреждения и ошибки консоли сайтов → %1").arg(CefRuntime.consoleLogPath())
                    keywords: "диагностика отладка console лог"
                    checked: Settings.debugConsoleLog
                    onToggled: (on) => Settings.debugConsoleLog = on
                }
                SAction {
                    label: qsTr("Диагностика звука и видео")
                    hint: qsTr("chrome://media-internals — что делает плеер и почему остановился")
                    keywords: "media internals звук видео плеер"
                    icon: "\uE8D6"; buttonText: qsTr("Открыть")
                    onTriggered: CefRuntime.openUrlRequest("chrome://media-internals/")
                }
                SSwitch {
                    label: qsTr("Предлагать сделать браузером по умолчанию")
                    checked: Settings.askDefaultBrowser
                    onToggled: (on) => Settings.askDefaultBrowser = on
                }
                Row_ {
                    label: "Crumb"
                    keywords: "версия chromium"
                    RowLayout {
                        spacing: 18
                        Image { source: "qrc:/crumb/icon.png"; sourceSize: Qt.size(128, 128); Layout.preferredWidth: 72; Layout.preferredHeight: 72; mipmap: true }
                        ColumnLayout {
                            spacing: 2
                            CText { text: "Crumb " + Qt.application.version; size: 20; font.weight: Font.DemiBold }
                            CText { text: qsTr("Движок Chromium 156 · интерфейс Qt %1").arg("6.10"); size: 13; color: Theme.onSurfaceVariant }
                            CText { text: qsTr("Шрифт Roboto Flex (OFL) · цвета Material Color Utilities"); size: 13; color: Theme.onSurfaceVariant }
                        }
                    }
                }
            }
        }
    }

    // ============================================================ диалоги
    FileDialog {
        id: homeImageDialog
        title: qsTr("Картинка для домашней страницы")
        nameFilters: [qsTr("Изображения (*.jpg *.jpeg *.png *.webp *.bmp)")]
        onAccepted: { Settings.homeImage = Settings.localFile(selectedFile); Settings.homeBackground = "image" }
    }
    FolderDialog {
        id: downloadsDialog
        title: qsTr("Папка для загрузок")
        onAccepted: Settings.downloadsDir = Settings.localFile(selectedFolder).replace(/\//g, "\\")
    }
    FileDialog {
        id: exportDialog
        title: qsTr("Экспорт настроек Crumb")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: [qsTr("Настройки Crumb (*.json)")]
        onAccepted: toast.show(Settings.exportTo(selectedFile) ? qsTr("Настройки сохранены") : qsTr("Не удалось сохранить файл"))
    }
    FileDialog {
        id: importDialog
        title: qsTr("Импорт настроек Crumb")
        nameFilters: [qsTr("Настройки Crumb (*.json)")]
        onAccepted: toast.show(Settings.importFrom(selectedFile) ? qsTr("Настройки загружены") : qsTr("Это не файл настроек Crumb"))
    }

    Popup {
        id: resetPopup
        anchors.centerIn: parent
        width: 440
        modal: true
        padding: 24
        background: Rectangle { color: Theme.surfaceContainerHigh; radius: 28 }
        contentItem: ColumnLayout {
            spacing: 14
            CText { text: qsTr("Сбросить все настройки?"); size: 22; font.weight: Font.DemiBold }
            CText {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Цвета, печеньки, вкладки, домашняя страница, горячие клавиши — всё вернётся к исходному виду.")
                color: Theme.onSurfaceVariant
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                Layout.topMargin: 6
                spacing: 18
                CookieButton { variant: "tonal"; icon: ""; text: qsTr("Отмена"); onClicked: resetPopup.close() }
                CookieButton {
                    icon: ""; text: qsTr("Сбросить")
                    variant: "filled"; color: Theme.error; contentColor: Theme.onError
                    onClicked: { Settings.resetAll(); resetPopup.close(); toast.show(qsTr("Все настройки сброшены")) }
                }
            }
        }
    }

    // Короткое уведомление внизу
    Rectangle {
        id: toast
        property string text
        function show(t) { text = t; opacity = 1; toastTimer.restart() }
        anchors.horizontalCenter: flick.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 28
        width: toastText.implicitWidth + 40
        height: 44
        radius: 22
        color: Theme.inverseSurface
        opacity: 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Prefs.dur(250) } }
        CText { id: toastText; anchors.centerIn: parent; text: toast.text; color: Theme.inverseOnSurface }
        Timer { id: toastTimer; interval: 2500; onTriggered: toast.opacity = 0 }
    }
}
