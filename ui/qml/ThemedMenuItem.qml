// Пункт меню Material 3 (строка списка, а не кнопка).
// Необязательно: значок слева (iconSource) и галочка справа (checked).

import QtQuick
import QtQuick.Controls.Basic

MenuItem {
    id: item
    property url iconSource: ""
    property string fontFamily: ""     // свой шрифт строки (список шрифтов)
    implicitHeight: 40
    implicitWidth: 240
    contentItem: Item {
        Image {
            id: ico
            visible: item.iconSource.toString().length > 0 && status === Image.Ready
            x: 8
            anchors.verticalCenter: parent.verticalCenter
            width: 18; height: 18
            source: item.iconSource
            sourceSize: Qt.size(36, 36)
            smooth: true
            asynchronous: true
        }
        Text {
            x: item.iconSource.toString().length > 0 ? 36 : 10
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - x - 30
            text: item.text
            elide: Text.ElideRight
            color: item.enabled ? Theme.onSurface : Qt.alpha(Theme.onSurface, 0.38)
            font.family: item.fontFamily.length ? item.fontFamily : Prefs.font
            font.pixelSize: Prefs.px(14)
        }
        Text {
            visible: item.checked
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: ""
            font.family: Theme.iconFont
            font.pixelSize: 13
            color: Theme.primary
        }
    }
    indicator: null
    background: Rectangle {
        radius: 10
        color: item.highlighted && item.enabled ? Qt.alpha(Theme.onSurface, 0.08) : "transparent"
    }
}
