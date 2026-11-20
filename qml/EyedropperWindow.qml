// Пипетка: снимок экрана во весь экран, лупа у курсора, клик — выбрать цвет, Esc — отмена.

import QtQuick
import Crumb
import Crumb.Ui

Window {
    id: picker
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
    color: "black"

    signal picked(color c)

    function start() {
        if (!Eyedropper.grab()) return
        x = Eyedropper.screenGeometry.x
        y = Eyedropper.screenGeometry.y
        width = Eyedropper.screenGeometry.width
        height = Eyedropper.screenGeometry.height
        showFullScreen()
        requestActivate()
        area.forceActiveFocus()
    }
    function finish() {
        hide()
        Eyedropper.release()
    }

    Image {
        id: shot
        anchors.fill: parent
        source: picker.visible ? Eyedropper.imageUrl : ""
        cache: false
        smooth: false
    }

    property point pos: Qt.point(width / 2, height / 2)
    readonly property color current: Eyedropper.colorAt(pos.x, pos.y)

    // Лупа: увеличенный кусок снимка вокруг курсора
    Item {
        id: loupe
        width: 150; height: 150
        x: Math.min(picker.width - width - 10, picker.pos.x + 24)
        y: Math.min(picker.height - height - 60, picker.pos.y + 24)
        clip: true
        Rectangle { anchors.fill: parent; radius: 75; color: "black" }
        Image {
            source: shot.source
            cache: false
            smooth: false
            width: shot.width * 8
            height: shot.height * 8
            x: -picker.pos.x * 8 + loupe.width / 2
            y: -picker.pos.y * 8 + loupe.height / 2
        }
        Rectangle {
            anchors.centerIn: parent
            width: 10; height: 10
            color: "transparent"
            border.color: "white"
            border.width: 2
        }
    }
    CookieShape {
        width: 56; height: 56
        x: loupe.x + loupe.width - 30
        y: loupe.y + loupe.height - 30
        color: picker.current
        strokeColor: "white"
        strokeWidth: 2
    }
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        y: 24
        width: hintText.implicitWidth + 32
        height: 40
        radius: 20
        color: Theme.inverseSurface
        CText {
            id: hintText
            anchors.centerIn: parent
            text: qsTr("Нажмите, чтобы взять цвет · Esc — отмена")
            color: Theme.inverseOnSurface
        }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.CrossCursor
        focus: true
        onPositionChanged: (e) => picker.pos = Qt.point(e.x, e.y)
        onClicked: (e) => {
            const c = Eyedropper.colorAt(e.x, e.y)
            picker.finish()
            picker.picked(c)
        }
        Keys.onEscapePressed: picker.finish()
    }
}
