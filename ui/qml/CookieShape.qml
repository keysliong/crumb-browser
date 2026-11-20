// Печенька Material 3 Expressive: круг с мягкими волнами по краю.
// Используется внутри CookieButton, CookieSwitch и как декор.
// Заливка — цвет или (если заданы gradientColors) диагональный градиент.

import QtQuick
import QtQuick.Shapes

Shape {
    id: root

    property int waves: 10
    property real depth: 0.10           // насколько глубоко «проваливаются» волны (доля радиуса)
    property color color: Theme.primary
    property var gradientColors: []     // 2–4 цвета: превью градиентного фона и т.п.
    property color strokeColor: "transparent"
    property real strokeWidth: 0
    property real spin: 0               // градусов в секунду; 0 — не вращается

    implicitWidth: 40
    implicitHeight: 40
    preferredRendererType: Shape.CurveRenderer

    function points(n, d, r) {
        const pts = []
        const steps = Math.max(64, n * 20)
        for (let i = 0; i <= steps; ++i) {
            const a = i / steps * Math.PI * 2
            const rr = r * (1 - d / 2 + d / 2 * Math.cos(n * a))
            pts.push(Qt.point(r + rr * Math.cos(a), r + rr * Math.sin(a)))
        }
        return pts
    }

    readonly property var pts: points(waves, depth, Math.min(width, height) / 2)
    readonly property bool useGradient: gradientColors && gradientColors.length >= 2
    function gradColor(i) {
        if (!useGradient) return root.color
        return gradientColors[Math.min(i, gradientColors.length - 1)]
    }

    ShapePath {
        fillColor: root.color
        fillGradient: root.useGradient ? grad : null
        strokeColor: root.strokeColor
        strokeWidth: root.strokeWidth
        PathPolyline { path: root.pts }
    }
    LinearGradient {
        id: grad
        x1: 0; y1: 0
        x2: root.width; y2: root.height
        GradientStop { position: 0.0; color: root.gradColor(0) }
        GradientStop { position: 0.5; color: root.gradColor(1) }
        GradientStop { position: 1.0; color: root.gradColor(root.useGradient ? root.gradientColors.length - 1 : 0) }
    }

    FrameAnimation {
        running: root.spin !== 0 && root.visible
        onTriggered: root.rotation = (root.rotation + frameTime * root.spin) % 360
    }
}
