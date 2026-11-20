// Поиск по странице (Ctrl+F): своя панель Crumb в правом верхнем углу страницы.
// Ищет по мере ввода, Enter / F3 — следующее, Shift+Enter — предыдущее, Esc — закрыть.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Crumb
import Crumb.Ui

CookiePopup {
    id: fb
    width: 420
    implicitWidth: 420
    padding: 8
    closePolicy: Popup.CloseOnEscape
    focus: true

    property var view: null          // BrowserView текущей вкладки
    property int total: 0
    property int active: 0
    property bool matchCase: false
    property bool searched: false
    property bool kicked: false        // первый поиск не выделяет совпадение — просим следующее
    signal closedByUser()

    background: Rectangle { color: Theme.surfaceContainerHigh; radius: 26; border.color: Theme.outlineVariant }

    function openFor(v, text) {
        if (view && view !== v) view.stopFinding(true)
        view = v
        if (text !== undefined && text.length) field.text = text
        if (!opened) open()
        field.forceActiveFocus()
        field.selectAll()
        if (field.text.length) run(false, true)
    }
    function run(next, forward) {
        if (!view) return
        if (!field.text.length) { view.find("", true, false, false); total = 0; active = 0; searched = false; return }
        searched = true
        if (!next) kicked = false
        view.find(field.text, forward, matchCase, next)
    }
    function next() { run(true, true) }
    function prev() { run(true, false) }

    onClosed: {
        if (view) view.stopFinding(false)   // выделение найденного остаётся, подсветка уходит
        total = 0; active = 0; searched = false
        closedByUser()
    }

    Connections {
        target: fb.view
        ignoreUnknownSignals: true
        function onFindResult(count, act, finalUpdate) {
            fb.total = count
            if (act > 0 || finalUpdate) fb.active = act
            if (finalUpdate && count > 0 && act === 0 && !fb.kicked) { fb.kicked = true; fb.view.find(field.text, true, fb.matchCase, true) }
        }
    }

    contentItem: RowLayout {
        spacing: 4
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            radius: 20
            color: Theme.surfaceContainerLowest
            border.width: 2
            border.color: fb.searched && fb.total === 0 ? Theme.error : Theme.primary
            Behavior on border.color { ColorAnimation { duration: Prefs.dur(150) } }
            Text {
                id: glass
                anchors.left: parent.left; anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                text: ""; font.family: Theme.iconFont; font.pixelSize: 13
                color: Theme.onSurfaceVariant
            }
            TextInput {
                id: field
                anchors.left: glass.right; anchors.leftMargin: 10
                anchors.right: counter.left; anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                font.family: Prefs.font
                font.pixelSize: Prefs.px(14)
                color: Theme.onSurface
                selectionColor: Theme.primaryContainer
                selectedTextColor: Theme.onPrimaryContainer
                selectByMouse: true
                clip: true
                onTextEdited: searchDelay.restart()
                Keys.onReturnPressed: (e) => { searchDelay.stop(); (e.modifiers & Qt.ShiftModifier) ? fb.prev() : fb.next() }
                Keys.onEnterPressed: (e) => { searchDelay.stop(); (e.modifiers & Qt.ShiftModifier) ? fb.prev() : fb.next() }
                Keys.onPressed: (e) => {
                    if (e.key === Qt.Key_F3) { e.accepted = true; (e.modifiers & Qt.ShiftModifier) ? fb.prev() : fb.next() }
                }
                CText {
                    anchors.fill: parent
                    verticalAlignment: Text.AlignVCenter
                    visible: field.text.length === 0
                    text: qsTr("Найти на странице")
                    color: Theme.onSurfaceVariant
                }
                Timer { id: searchDelay; interval: 120; onTriggered: fb.run(false, true) }
            }
            CText {
                id: counter
                anchors.right: parent.right; anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                size: 12.5
                font.weight: Font.Medium
                color: fb.searched && fb.total === 0 ? Theme.error : Theme.onSurfaceVariant
                text: !fb.searched ? "" : fb.total === 0 ? qsTr("Нет совпадений") : qsTr("%1 из %2").arg(fb.active).arg(fb.total)
            }
        }
        CookieButton {
            size: 32; variant: "surface"
            letter: "Aa"
            checked: fb.matchCase
            toolTip: qsTr("Учитывать регистр")
            onClicked: { fb.matchCase = !fb.matchCase; fb.run(false, true); field.forceActiveFocus() }
        }
        CookieButton {
            size: 32; variant: "surface"; icon: ""
            enabled: fb.total > 0
            toolTip: qsTr("Предыдущее (Shift+Enter)")
            onClicked: { fb.prev(); field.forceActiveFocus() }
        }
        CookieButton {
            size: 32; variant: "surface"; icon: ""
            enabled: fb.total > 0
            toolTip: qsTr("Следующее (Enter)")
            onClicked: { fb.next(); field.forceActiveFocus() }
        }
        CookieButton {
            size: 32; variant: "surface"; icon: ""
            toolTip: qsTr("Закрыть (Esc)")
            onClicked: fb.close()
        }
    }
}
