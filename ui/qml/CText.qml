// Текст интерфейса Crumb: шрифт и масштаб из настроек (Prefs), цвет из темы.

import QtQuick

Text {
    property real size: 14
    font.family: Prefs.font
    font.pixelSize: Prefs.px(size)
    color: Theme.onSurface
}
