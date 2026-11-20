// Контекстное меню в стиле Material 3. Открывается отдельным окном, чтобы
// появляться поверх страницы (CEF рисует её в собственном окне).
// Пункты — ThemedMenuItem, разделители — ThemedMenuSeparator.

import QtQuick
import QtQuick.Controls.Basic

Menu {
    id: menu
    popupType: Popup.Window
    padding: 6

    background: Rectangle {
        implicitWidth: 240
        color: Theme.surfaceContainer
        radius: 14
        border.color: Theme.outlineVariant
        border.width: 1
    }
}
