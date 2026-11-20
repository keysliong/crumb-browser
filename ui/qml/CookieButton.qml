// CookieButton — единственная кнопка во всём Crumb.
// Форма «печенька»; при наведении она медленно вращается, при нажатии волны
// «вдыхают» и кнопка пружинит. Текст, если нужен, стоит рядом или под печенькой.
// Форма, размер, анимации и звук берутся из настроек (Prefs) — меняются вживую.

import QtQuick
import QtQuick.Controls.Basic

Item {
    id: root

    // --- содержимое
    property string icon: ""                 // глиф Segoe Fluent Icons
    property string letter: ""               // или буква обычным шрифтом (запасной значок сайта)
    property url iconSource: ""              // или картинка (иконка сайта и т.п.)
    property string text: ""
    property string toolTip: ""
    property string labelPosition: "right"   // right | below
    property int labelWidth: 0               // >0 — подпись под печенькой обрезается по ширине

    // --- вид: filled | tonal | outlined | surface
    property string variant: "tonal"
    property bool checked: false             // для переключаемых кнопок: checked → filled

    // --- форма (по умолчанию — из настроек)
    property real size: 40
    property int waves: Prefs.cookieWaves
    property real depth: Prefs.cookieDepth
    property real spinSpeed: Prefs.cookieSpin
    property bool breathe: Prefs.cookieBreathe
    readonly property real realSize: Math.round(size * Prefs.cookieScale)

    // --- цвета можно переопределить
    readonly property string effectiveVariant: checked ? "filled" : variant
    property color color: effectiveVariant === "filled" ? Theme.primary
                        : effectiveVariant === "tonal" ? Theme.secondaryContainer
                        : effectiveVariant === "surface" ? Theme.surfaceContainerHighest
                        : "transparent"
    property color contentColor: effectiveVariant === "filled" ? Theme.onPrimary
                               : effectiveVariant === "tonal" ? Theme.onSecondaryContainer
                               : effectiveVariant === "outlined" ? Theme.primary
                               : Theme.onSurfaceVariant
    property color labelColor: Theme.onSurface
    property var gradientColors: []          // заливка градиентом (превью фонов)

    signal clicked()

    readonly property bool hovered: hover.hovered
    readonly property bool pressed: tap.pressed

    function activate() {
        Prefs.click()
        root.clicked()
    }

    implicitWidth: labelPosition === "below"
                   ? Math.max(realSize, label.visible ? label.width : 0)
                   : realSize + (label.visible ? 10 + label.implicitWidth : 0)
    implicitHeight: labelPosition === "below"
                    ? realSize + (label.visible ? 6 + label.implicitHeight : 0)
                    : Math.max(realSize, label.implicitHeight)
    opacity: enabled ? 1 : 0.38
    Behavior on opacity { NumberAnimation { duration: Prefs.dur(150) } }
    activeFocusOnTab: true

    Accessible.role: Accessible.Button
    Accessible.name: text.length ? text : toolTip
    Accessible.onPressAction: root.activate()
    Keys.onSpacePressed: root.activate()
    Keys.onReturnPressed: root.activate()

    // Глубина волн: при нажатии «вдох».
    property real liveDepth: depth * (breathe && pressed ? 1.9 : 1.0)
    Behavior on liveDepth {
        enabled: Prefs.animations
        SpringAnimation { spring: Prefs.spring(4); damping: 0.2; epsilon: 0.001 }
    }
    // Скорость вращения разгоняется и тормозит плавно.
    property real liveSpin: hovered && enabled && Prefs.animations ? spinSpeed : 0
    Behavior on liveSpin { NumberAnimation { duration: Prefs.dur(700); easing.type: Easing.OutCubic } }

    Item {
        id: cookie
        width: root.realSize
        height: root.realSize
        x: root.labelPosition === "below" ? (root.width - width) / 2 : 0
        y: root.labelPosition === "below" ? 0 : (root.height - height) / 2
        scale: root.pressed ? 0.88 : (root.hovered ? 1.05 : 1.0)
        Behavior on scale {
            enabled: Prefs.animations
            SpringAnimation { spring: Prefs.spring(5); damping: 0.22; epsilon: 0.002 }
        }

        CookieShape {
            id: shape
            anchors.fill: parent
            waves: root.waves
            depth: root.liveDepth
            color: root.color
            gradientColors: root.gradientColors
            strokeColor: root.effectiveVariant === "outlined" ? Theme.outline : "transparent"
            strokeWidth: root.effectiveVariant === "outlined" ? 1.2 : 0
            spin: root.liveSpin
            Behavior on color { ColorAnimation { duration: Prefs.dur(200) } }
        }
        // State layer Material 3: 8% наведение/фокус, 12% нажатие.
        CookieShape {
            anchors.fill: parent
            rotation: shape.rotation
            waves: root.waves
            depth: root.liveDepth
            color: Qt.alpha(root.contentColor,
                            root.pressed ? 0.12 : (root.hovered || root.activeFocus ? 0.08 : 0))
            strokeColor: root.activeFocus ? root.contentColor : "transparent"
            strokeWidth: root.activeFocus ? 2 : 0
        }

        Image {
            id: img
            anchors.centerIn: parent
            visible: status === Image.Ready
            source: root.iconSource
            width: Math.round(root.realSize * 0.5)
            height: width
            sourceSize: Qt.size(64, 64)
            smooth: true
            asynchronous: true
        }
        Text {
            anchors.centerIn: parent
            visible: !img.visible && (root.icon.length > 0 || root.letter.length > 0)
            text: root.letter.length > 0 ? root.letter : root.icon
            font.family: root.letter.length > 0 ? Prefs.font : Theme.iconFont
            font.pixelSize: Math.round(root.realSize * (root.letter.length > 0 ? 0.42 : 0.4))
            font.weight: root.letter.length > 0 ? Font.DemiBold : Font.Normal
            color: root.contentColor
        }
    }

    Text {
        id: label
        visible: root.text.length > 0
        text: root.text
        color: root.labelColor
        font.family: Prefs.font
        font.pixelSize: Prefs.px(14)
        font.weight: Font.Medium
        width: root.labelPosition === "below" && root.labelWidth > 0 ? root.labelWidth : implicitWidth
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        x: root.labelPosition === "below" ? (root.width - width) / 2 : cookie.width + 10
        y: root.labelPosition === "below" ? cookie.height + 6 : (root.height - implicitHeight) / 2
    }

    HoverHandler {
        id: hover
        cursorShape: Prefs.handCursor ? Qt.PointingHandCursor : Qt.ArrowCursor
        enabled: root.enabled
    }
    TapHandler {
        id: tap
        enabled: root.enabled
        gesturePolicy: TapHandler.WithinBounds
        onTapped: root.activate()
    }

    // Отдельное окно-подсказка: обычный QML-попап не может появиться поверх страницы.
    ToolTip {
        id: tip
        popupType: Popup.Window
        visible: root.toolTip.length > 0 && root.hovered && !root.pressed
        delay: 600
        text: root.toolTip
        contentItem: Text {
            text: tip.text
            color: Theme.inverseOnSurface
            font.family: Prefs.font
            font.pixelSize: Prefs.px(12)
        }
        background: Rectangle { color: Theme.inverseSurface; radius: 6 }
    }
}
