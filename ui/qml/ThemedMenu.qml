// Контекстное меню в стиле Material 3. Открывается отдельным окном, чтобы
// появляться поверх страницы (CEF рисует её в собственном окне).
// Пункты — ThemedMenuItem, разделители — ThemedMenuSeparator.

import QtQuick
import QtQuick.Controls.Basic

Menu {
    id: menu
    popupType: Popup.Window
    padding: 6
    // Меню раскрывается от точки нажатия с пружиной
    transformOrigin: Popup.TopLeft
    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Prefs.dur(120) }
            NumberAnimation { property: "scale"; from: 0.9; to: 1; duration: Prefs.dur(260); easing.type: Easing.OutBack }
        }
    }
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Prefs.dur(100) } }

    background: Rectangle {
        implicitWidth: 240
        color: Theme.surfaceContainer
        radius: 14
        border.color: Theme.outlineVariant
        border.width: 1
    }
}
