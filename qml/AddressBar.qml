// Адресная строка. Поле ввода — не кнопка, поэтому оно скруглённое, а не печенька.

import QtQuick
import Crumb.Ui

Rectangle {
    id: bar

    property string url: ""
    property bool secure: false
    property bool loading: false
    readonly property bool editing: input.activeFocus

    signal submitted(string text)
    signal editingStarted()
    signal editingFinished()
    signal typed(string text)          // человек изменил текст (для подсказок)
    signal arrow(int step)             // ↑/↓ — по подсказкам

    // Пока клавиатура у страницы (окно CEF), Qt считает своё окно неактивным и
    // forceActiveFocus() лишь «помечает» поле. Поэтому сначала просим забрать
    // фокус у страницы (editingStarted → BrowserView.releasePageFocus), потом фокусируем.
    function focusAddress() {
        bar.editingStarted()
        input.forceActiveFocus()
        input.selectAll()
    }

    // Выйти из режима ввода (например, при переключении вкладки).
    function cancelEditing() {
        input.focus = false
        showUrl()
    }

    function showUrl() {
        input.text = url
        input.cursorPosition = 0   // начало адреса (домен) всегда видно
    }

    implicitHeight: 44
    radius: height / 2
    color: editing ? Theme.surfaceContainerLowest : Theme.surfaceContainerHigh
    border.width: editing ? 2 : 0
    border.color: Theme.primary
    Behavior on color { ColorAnimation { duration: 180 } }

    onUrlChanged: if (!input.activeFocus) showUrl()

    // Клик мышью по строке, когда клавиатура была у страницы, — то же самое.
    TapHandler {
        gesturePolicy: TapHandler.DragThreshold
        onPressedChanged: if (pressed && !input.activeFocus) bar.editingStarted()
    }

    Text {
        id: glyph
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        font.family: Theme.iconFont
        font.pixelSize: 15
        color: Theme.onSurfaceVariant
        // Замок для https, лупа при вводе, глобус для остального
        text: bar.editing ? "" : (bar.secure ? "" : "")
    }

    TextInput {
        id: input
        anchors.left: glyph.right
        anchors.leftMargin: 12
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        clip: true
        selectByMouse: true
        font.family: Prefs.font
        font.pixelSize: Prefs.px(15)
        color: Theme.onSurface
        selectionColor: Theme.primaryContainer
        selectedTextColor: Theme.onPrimaryContainer
        inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText

        Text {
            anchors.fill: parent
            verticalAlignment: Text.AlignVCenter
            visible: input.text.length === 0
            text: qsTr("Введите адрес или запрос")
            font.family: Prefs.font
            color: Theme.onSurfaceVariant
            font.pixelSize: input.font.pixelSize
        }

        onActiveFocusChanged: {
            if (activeFocus) {
                bar.editingStarted()
                Qt.callLater(selectAll)
            } else {
                bar.showUrl()
                bar.editingFinished()
            }
        }
        onTextEdited: bar.typed(text)
        Keys.onUpPressed: bar.arrow(-1)
        Keys.onDownPressed: bar.arrow(1)
        onAccepted: {
            bar.submitted(text)
            focus = false
        }
        Keys.onEscapePressed: {
            bar.showUrl()
            focus = false
        }
    }
}
