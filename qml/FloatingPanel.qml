// Плавающая панель поверх страницы, которая НЕ забирает фокус клавиатуры:
// подсказки адресной строки (печатаете дальше в строке) и подстановка паролей
// (фокус остаётся в поле сайта). Отдельное окно — QML не рисует поверх CEF.
import QtQuick
import Crumb.Ui

Window {
    id: panel
    flags: Qt.ToolTip | Qt.FramelessWindowHint | Qt.WindowDoesNotAcceptFocus | Qt.NoDropShadowWindowHint
    color: "transparent"
    visible: false

    default property alias content: body.data
    property real radius: 22
    property bool shown: false
    readonly property real padding: 8

    // Показать под точкой (x, y) в координатах экрана.
    function showAt(gx, gy, w, h) {
        width = w
        height = h
        x = Math.round(gx)
        y = Math.round(gy)
        if (!shown) {
            shown = true
            visible = true
            if (Prefs.animations) enterAnim.restart()
            else { card.opacity = 1; card.scale = 1 }
        }
    }
    function hide() {
        if (!shown) return
        shown = false
        visible = false
    }

    Rectangle {
        id: card
        anchors.fill: parent
        anchors.margins: 2
        radius: panel.radius
        color: Theme.surfaceContainer
        border.color: Theme.outlineVariant
        transformOrigin: Item.Top
        Item {
            id: body
            anchors.fill: parent
            anchors.margins: panel.padding
        }
    }
    ParallelAnimation {
        id: enterAnim
        NumberAnimation { target: card; property: "opacity"; from: 0; to: 1; duration: Prefs.dur(120) }
        NumberAnimation { target: card; property: "scale"; from: 0.94; to: 1; duration: Prefs.dur(260); easing.type: Easing.OutBack; easing.overshoot: 1.1 * Prefs.springStrength }
    }
}
