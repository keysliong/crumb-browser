// Пароли (crumb://passwords): сохранённые входы, поиск, добавление, генератор.
// Показать, скопировать или изменить пароль можно после Windows Hello —
// доступ открыт 5 минут, потом снова закрывается.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Crumb
import Crumb.Ui

Item {
    id: page

    signal openUrl(string url)

    Rectangle { anchors.fill: parent; color: Theme.surface }

    property var items: { Passwords.revision; return Passwords.list(search.text) }
    property var shown: ({})              // id → пароль, раскрытые строки
    property var pending: null            // действие, ждущее Windows Hello
    property string toastText: ""

    // Выполнить действие, которому нужна проверка личности.
    function withUnlock(fn) {
        if (Passwords.unlocked) { fn(); return }
        pending = fn
        Passwords.unlock(Window.window)
    }
    Connections {
        target: Passwords
        function onUnlockedChanged() {
            if (Passwords.unlocked && page.pending) { const f = page.pending; page.pending = null; f() }
            if (!Passwords.unlocked) page.shown = ({})
        }
        function onUnlockFailed() { page.pending = null; page.say(qsTr("Не удалось подтвердить личность")) }
    }
    function toggleShow(id) {
        withUnlock(() => {
            const s = Object.assign({}, shown)
            if (s[id] !== undefined) delete s[id]; else s[id] = Passwords.password(id)
            shown = s
        })
    }
    function say(t) { toastText = t; toastAnim.restart() }

    ColumnLayout {
        id: header
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 28
        width: Math.min(parent.width - 64, 860)
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 14
            CookieShape {
                width: 48; height: 48
                color: Theme.primaryContainer
                Text { anchors.centerIn: parent; text: ""; font.family: Theme.iconFont; font.pixelSize: 20; color: Theme.onPrimaryContainer }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                CText { text: qsTr("Пароли"); size: 28; font.weight: Font.Bold }
                CText {
                    text: qsTr("Сохранено: %1 · зашифрованы Windows, хранятся только на этом компьютере").arg(Passwords.count)
                    size: 13; color: Theme.onSurfaceVariant
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
            CookieButton {
                icon: Passwords.unlocked ? "" : ""
                variant: "surface"
                text: Passwords.verifying ? qsTr("Проверка…") : Passwords.unlocked ? qsTr("Закрыть доступ") : qsTr("Открыть доступ")
                toolTip: Passwords.unlocked ? qsTr("Пароли снова будут скрыты") : qsTr("Windows Hello или пароль Windows")
                onClicked: Passwords.unlocked ? Passwords.lock() : Passwords.unlock(Window.window)
            }
            CookieButton { icon: ""; text: qsTr("Добавить"); variant: "filled"; onClicked: editor.openNew() }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            radius: 24
            color: Theme.surfaceContainerHigh
            border.width: search.activeFocus ? 2 : 0
            border.color: Theme.primary
            Text {
                id: g
                anchors.left: parent.left; anchors.leftMargin: 18
                anchors.verticalCenter: parent.verticalCenter
                text: ""; font.family: Theme.iconFont; font.pixelSize: 15; color: Theme.onSurfaceVariant
            }
            TextInput {
                id: search
                anchors.left: g.right; anchors.leftMargin: 12
                anchors.right: parent.right; anchors.rightMargin: 18
                anchors.verticalCenter: parent.verticalCenter
                font.family: Prefs.font; font.pixelSize: Prefs.px(15)
                color: Theme.onSurface
                selectByMouse: true
                selectionColor: Theme.primaryContainer
                clip: true
                CText {
                    anchors.fill: parent; verticalAlignment: Text.AlignVCenter
                    visible: search.text.length === 0
                    text: qsTr("Искать по сайту или имени пользователя")
                    color: Theme.onSurfaceVariant
                }
            }
        }
    }

    // Пусто
    ColumnLayout {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: 40
        visible: page.items.length === 0
        spacing: 12
        CookieShape {
            Layout.alignment: Qt.AlignHCenter
            width: 96; height: 96
            color: Theme.surfaceContainerHigh
            spin: Prefs.animations ? 12 : 0
            Text { anchors.centerIn: parent; rotation: -parent.rotation; text: ""; font.family: Theme.iconFont; font.pixelSize: 36; color: Theme.onSurfaceVariant }
        }
        CText {
            Layout.alignment: Qt.AlignHCenter
            text: search.text.length ? qsTr("Ничего не нашлось") : qsTr("Паролей пока нет")
            size: 18; font.weight: Font.Medium
        }
        CText {
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: 420
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("Войдите на любой сайт — Crumb предложит запомнить пароль и в следующий раз подставит его сам")
            color: Theme.onSurfaceVariant
        }
    }

    ListView {
        id: list
        anchors.top: header.bottom
        anchors.topMargin: 16
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: header.width
        clip: true
        spacing: 8
        model: page.items
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        footer: Item {
            width: list.width
            height: neverCol.visible ? neverCol.implicitHeight + 40 : 30
            ColumnLayout {
                id: neverCol
                visible: (Settings.passwordsNever || []).length > 0 && search.text.length === 0
                y: 24
                width: parent.width
                spacing: 8
                CText { text: qsTr("Никогда не сохранять для этих сайтов"); size: 14; font.weight: Font.DemiBold; color: Theme.onSurfaceVariant }
                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    Repeater {
                        model: Settings.passwordsNever || []
                        Rectangle {
                            required property string modelData
                            height: 36; radius: 18
                            width: nr.implicitWidth + 20
                            color: Theme.surfaceContainerHigh
                            RowLayout {
                                id: nr
                                anchors.centerIn: parent
                                spacing: 6
                                CText { text: modelData; size: 13 }
                                CookieButton { size: 24; variant: "surface"; icon: ""; toolTip: qsTr("Снова предлагать"); onClicked: Passwords.allowSite(modelData) }
                            }
                        }
                    }
                }
            }
        }

        delegate: Rectangle {
            id: card
            required property var modelData
            readonly property int pid: modelData.id
            readonly property bool open: page.shown[pid] !== undefined
            width: list.width
            height: 68
            radius: 22
            color: hov.hovered ? Theme.surfaceContainerHigh : Theme.surfaceContainerLow
            Behavior on color { ColorAnimation { duration: Prefs.dur(120) } }
            HoverHandler { id: hov }

            CookieShape {
                id: ic
                x: 14
                anchors.verticalCenter: parent.verticalCenter
                width: 40; height: 40
                color: Theme.surfaceContainerHighest
                Image {
                    anchors.centerIn: parent
                    visible: card.modelData.icon.length > 0
                    width: 20; height: 20
                    source: card.modelData.icon
                    sourceSize: Qt.size(40, 40)
                    mipmap: true
                }
                CText {
                    anchors.centerIn: parent
                    visible: card.modelData.icon.length === 0
                    text: (card.modelData.site || "?")[0].toUpperCase()
                    size: 16; font.weight: Font.DemiBold
                    color: Theme.onSurfaceVariant
                }
            }
            ColumnLayout {
                anchors.left: ic.right; anchors.leftMargin: 14
                anchors.right: actions.left; anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                RowLayout {
                    spacing: 8
                    CText {
                        text: card.modelData.site
                        size: 14.5; font.weight: Font.Medium
                        elide: Text.ElideRight
                        Layout.maximumWidth: 260
                        TapHandler { onTapped: page.openUrl(card.modelData.origin || ("https://" + card.modelData.site)) }
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                    }
                    CText {
                        text: card.modelData.username.length ? card.modelData.username : qsTr("без имени")
                        size: 13.5
                        color: card.modelData.username.length ? Theme.onSurface : Theme.onSurfaceVariant
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
                CText {
                    text: card.open ? page.shown[card.pid] : "•••••••••••• · " + qsTr("изменён %1").arg(card.modelData.updated)
                    size: card.open ? 14 : 12.5
                    font.family: card.open ? "Consolas" : Prefs.font
                    color: card.open ? Theme.primary : Theme.onSurfaceVariant
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
            Row {
                id: actions
                anchors.right: parent.right; anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4
                CookieButton {
                    size: 34; variant: "surface"
                    icon: card.open ? "" : ""
                    toolTip: card.open ? qsTr("Скрыть пароль") : qsTr("Показать пароль")
                    onClicked: page.toggleShow(card.pid)
                }
                CookieButton {
                    size: 34; variant: "surface"; icon: ""
                    toolTip: qsTr("Копировать пароль (сотрётся из буфера через 30 с)")
                    onClicked: page.withUnlock(() => { if (Passwords.copyPassword(card.pid)) page.say(qsTr("Пароль скопирован")) })
                }
                CookieButton {
                    size: 34; variant: "surface"; icon: ""
                    visible: card.modelData.username.length > 0
                    toolTip: qsTr("Копировать имя пользователя")
                    onClicked: { Passwords.copyText(card.modelData.username); page.say(qsTr("Имя пользователя скопировано")) }
                }
                CookieButton {
                    size: 34; variant: "surface"; icon: ""
                    toolTip: qsTr("Изменить")
                    onClicked: page.withUnlock(() => editor.openEdit(card.modelData))
                }
                CookieButton {
                    size: 34; variant: "surface"; icon: ""
                    toolTip: qsTr("Удалить")
                    onClicked: { confirmDel.target = card.modelData; confirmDel.open() }
                }
            }
        }
    }

    // ---- подтверждение удаления
    CookiePopup {
        id: confirmDel
        property var target: null
        parent: page
        anchors.centerIn: parent
        transformOrigin: Popup.Center
        modal: true
        padding: 22
        width: 360
        background: Rectangle { color: Theme.surfaceContainerHigh; radius: 28 }
        contentItem: ColumnLayout {
            spacing: 14
            CText { text: qsTr("Удалить пароль?"); size: 18; font.weight: Font.DemiBold }
            CText {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.onSurfaceVariant
                text: confirmDel.target ? qsTr("%1 на %2. Это нельзя отменить.").arg(confirmDel.target.username || qsTr("Вход")).arg(confirmDel.target.site) : ""
            }
            RowLayout {
                Item { Layout.fillWidth: true }
                CookieButton { size: 34; variant: "surface"; icon: ""; text: qsTr("Отмена"); onClicked: confirmDel.close() }
                CookieButton {
                    size: 34; icon: ""; text: qsTr("Удалить")
                    color: Theme.error; contentColor: Theme.onError
                    onClicked: { Passwords.remove(confirmDel.target.id); confirmDel.close(); page.say(qsTr("Пароль удалён")) }
                }
            }
        }
    }

    // ---- добавление / изменение
    CookiePopup {
        id: editor
        property int editId: -1
        property bool reveal: true
        parent: page
        anchors.centerIn: parent
        transformOrigin: Popup.Center
        modal: true
        padding: 22
        width: 440
        background: Rectangle { color: Theme.surfaceContainerHigh; radius: 28 }

        function openNew() {
            editId = -1; reveal = true
            siteFieldBox.input.text = ""; userFieldBox.input.text = ""; passFieldBox.input.text = Passwords.generate()
            open(); siteFieldBox.input.forceActiveFocus()
        }
        function openEdit(m) {
            editId = m.id; reveal = false
            siteFieldBox.input.text = m.site; userFieldBox.input.text = m.username; passFieldBox.input.text = Passwords.password(m.id)
            open(); userFieldBox.input.forceActiveFocus()
        }
        function apply() {
            if (!passFieldBox.input.text.length || (editId < 0 && !siteFieldBox.input.text.trim().length)) return
            if (editId >= 0) Passwords.update(editId, userFieldBox.input.text.trim(), passFieldBox.input.text)
            else Passwords.add(siteFieldBox.input.text.trim(), userFieldBox.input.text.trim(), passFieldBox.input.text)
            close()
            page.say(editId >= 0 ? qsTr("Сохранено") : qsTr("Пароль добавлен"))
        }

        component EField: Rectangle {
            property alias input: ti
            property string placeholder
            property string glyph
            property bool secret: false
            property bool readOnly: false
            default property alias extra: tail.data
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            radius: 23
            color: Theme.surfaceContainerHighest
            border.width: ti.activeFocus ? 2 : 0
            border.color: Theme.primary
            Text {
                id: gl
                anchors.left: parent.left; anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: parent.glyph; font.family: Theme.iconFont; font.pixelSize: 13; color: Theme.onSurfaceVariant
            }
            TextInput {
                id: ti
                anchors.left: gl.right; anchors.leftMargin: 12
                anchors.right: tail.left; anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                font.family: Prefs.font; font.pixelSize: Prefs.px(14)
                color: Theme.onSurface
                readOnly: parent.readOnly
                echoMode: parent.secret ? TextInput.Password : TextInput.Normal
                selectByMouse: true
                selectionColor: Theme.primaryContainer
                clip: true
                onAccepted: editor.apply()
                CText {
                    anchors.fill: parent; verticalAlignment: Text.AlignVCenter
                    visible: ti.text.length === 0
                    text: parent.parent.placeholder
                    color: Theme.onSurfaceVariant
                }
            }
            Row { id: tail; anchors.right: parent.right; anchors.rightMargin: 6; anchors.verticalCenter: parent.verticalCenter; spacing: 2 }
        }

        contentItem: ColumnLayout {
            spacing: 12
            CText { text: editor.editId >= 0 ? qsTr("Изменить пароль") : qsTr("Новый пароль"); size: 18; font.weight: Font.DemiBold }
            EField { id: siteFieldBox; readOnly: editor.editId >= 0; glyph: ""; placeholder: qsTr("Сайт, например github.com"); opacity: editor.editId >= 0 ? 0.6 : 1 }
            EField { id: userFieldBox; glyph: ""; placeholder: qsTr("Имя пользователя или почта") }
            EField {
                id: passFieldBox
                glyph: ""; placeholder: qsTr("Пароль")
                secret: !editor.reveal
                CookieButton { size: 32; variant: "surface"; icon: editor.reveal ? "" : ""; toolTip: editor.reveal ? qsTr("Скрыть") : qsTr("Показать"); onClicked: editor.reveal = !editor.reveal }
                CookieButton { size: 32; variant: "surface"; icon: ""; toolTip: qsTr("Придумать надёжный пароль"); onClicked: { passFieldBox.input.text = Passwords.generate(); editor.reveal = true } }
            }
            // Надёжность
            RowLayout {
                spacing: 6
                readonly property int s: Passwords.strength(passFieldBox.input.text)
                Repeater {
                    model: 4
                    Rectangle {
                        required property int index
                        Layout.preferredWidth: 40; Layout.preferredHeight: 6; radius: 3
                        color: index <= parent.s && passFieldBox.input.text.length
                               ? (parent.s === 0 ? Theme.error : parent.s === 1 ? "#e0a030" : Theme.primary)
                               : Theme.surfaceContainerHighest
                        Behavior on color { ColorAnimation { duration: Prefs.dur(160) } }
                    }
                }
                CText {
                    Layout.leftMargin: 6
                    size: 12.5; color: Theme.onSurfaceVariant
                    text: !passFieldBox.input.text.length ? "" : [qsTr("слабый"), qsTr("так себе"), qsTr("хороший"), qsTr("надёжный")][parent.s]
                }
            }
            RowLayout {
                Layout.topMargin: 4
                Item { Layout.fillWidth: true }
                CookieButton { size: 34; variant: "surface"; icon: ""; text: qsTr("Отмена"); onClicked: editor.close() }
                CookieButton { size: 34; variant: "filled"; icon: ""; text: qsTr("Сохранить"); onClicked: editor.apply() }
            }
        }
    }

    // ---- короткое уведомление
    Rectangle {
        id: toast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom; anchors.bottomMargin: 28
        width: tt.implicitWidth + 40; height: 44; radius: 22
        color: Theme.inverseSurface
        opacity: 0
        visible: opacity > 0
        CText { id: tt; anchors.centerIn: parent; text: page.toastText; color: Theme.inverseOnSurface; size: 14 }
        SequentialAnimation {
            id: toastAnim
            NumberAnimation { target: toast; property: "opacity"; to: 1; duration: Prefs.dur(180) }
            PauseAnimation { duration: 2600 }
            NumberAnimation { target: toast; property: "opacity"; to: 0; duration: Prefs.dur(260) }
        }
    }
}
