// Левая панель установщика (как в образце): плавающие печеньки, эмблема,
// волнистое кольцо прогресса, в конце — печенька с галочкой.

import QtQuick
import QtQuick.Shapes
import Crumb.Ui

Rectangle {
    id: hero

    property real time: 0
    property real progress: -1          // < 0 — кольца нет
    property real doneT: 0              // 0…1 — появление галочки
    property bool uninstall: false
    property string version: ""

    color: Theme.primaryContainer
    radius: 14
    clip: true

    readonly property real cx: width / 2
    readonly property real cy: height / 2 - 30 + Math.sin(time * 1.3) * 6
    readonly property real bounce: doneT > 0 ? 1 + 0.16 * Math.sin(doneT * Math.PI) : 1
    readonly property real logoR: 74 * bounce
    readonly property real fade: uninstall ? Math.max(0, Math.min(1, progress)) : 0

    // Плавающие декоративные печеньки: [x, y, радиус, волны, вращение, скорость]
    Repeater {
        model: [[0.18, 0.16, 34, 7, 0.35, 0.7], [0.82, 0.22, 22, 6, -0.5, 1.1],
                [0.12, 0.80, 26, 9, 0.6, 0.9], [0.86, 0.74, 40, 8, -0.3, 0.6],
                [0.55, 0.92, 16, 5, 0.8, 1.3]]
        CookieShape {
            required property var modelData
            required property int index
            readonly property real ph: hero.time * modelData[5] + index
            width: modelData[2] * 2
            height: width
            x: hero.width * modelData[0] + Math.sin(ph) * 8 - width / 2
            y: hero.height * modelData[1] + Math.cos(ph * 0.8) * 10 - height / 2
            waves: modelData[3]
            depth: 0.14
            rotation: hero.time * modelData[4] * 57.3
            color: Qt.alpha(Theme.primary, 0.13)
        }
    }

    // Волнистое кольцо прогресса (Material 3 Expressive)
    function arc(r, sweep, amp, waves, phase) {
        const pts = []
        const n = Math.max(8, Math.floor(Math.abs(sweep) * r / 2.5))
        for (let i = 0; i <= n; ++i) {
            const a = -Math.PI / 2 + sweep * i / n
            const rr = r + amp * Math.sin(waves * a + phase)
            pts.push(Qt.point(hero.cx + Math.cos(a) * rr, hero.cy + Math.sin(a) * rr))
        }
        return pts
    }
    Shape {
        anchors.fill: parent
        visible: hero.progress >= 0
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: Qt.alpha(Theme.primary, 0.18)
            strokeWidth: 5
            fillColor: "transparent"
            PathPolyline { path: hero.arc(104, Math.PI * 2, 0, 0, 0) }
        }
        ShapePath {
            strokeColor: Theme.primary
            strokeWidth: 5.5
            capStyle: ShapePath.RoundCap
            fillColor: "transparent"
            PathPolyline {
                path: hero.progress > 0.001
                      ? hero.arc(104, Math.PI * 2 * Math.min(1, hero.progress), 3.5 * (1 - hero.doneT), 14, -hero.time * 5)
                      : []
            }
        }
    }

    // Иконка Crumb (как в образце — иконка приложения, медленно покачивается;
    // при удалении уменьшается и бледнеет по мере прогресса).
    Image {
        id: logo
        width: hero.logoR * 2.15 * (1 - 0.35 * hero.fade)
        height: width
        x: hero.cx - width / 2
        y: hero.cy - height / 2
        rotation: (Math.sin(hero.time * 0.9) * 0.12 + hero.fade * 0.6) * 57.3
        opacity: 1 - 0.65 * hero.fade
        source: "qrc:/crumb/icon.png"
        sourceSize: Qt.size(256, 256)
        smooth: true
        mipmap: true
    }

    // В конце поверх проявляется печенька с галочкой
    CookieShape {
        visible: hero.doneT > 0
        width: hero.logoR * 2.04
        height: width
        x: hero.cx - width / 2
        y: hero.cy - height / 2
        waves: 12
        depth: 0.075
        rotation: logo.rotation
        color: Qt.alpha(Theme.inversePrimary, hero.doneT)
    }
    Shape {
        anchors.fill: parent
        visible: hero.doneT > 0
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: Theme.onPrimaryContainer
            strokeWidth: 11
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            fillColor: "transparent"
            PathPolyline {
                // Галочка рисуется «по частям» по мере анимации
                path: {
                    const p0 = Qt.point(hero.cx - 28, hero.cy + 2)
                    const p1 = Qt.point(hero.cx - 8, hero.cy + 22)
                    const p2 = Qt.point(hero.cx + 30, hero.cy - 18)
                    const l1 = Math.hypot(p1.x - p0.x, p1.y - p0.y)
                    const l2 = Math.hypot(p2.x - p1.x, p2.y - p1.y)
                    const len = (l1 + l2) * hero.doneT
                    if (len <= l1) {
                        const k = len / l1
                        return [p0, Qt.point(p0.x + (p1.x - p0.x) * k, p0.y + (p1.y - p0.y) * k)]
                    }
                    const k = Math.min(1, (len - l1) / l2)
                    return [p0, p1, Qt.point(p1.x + (p2.x - p1.x) * k, p1.y + (p2.y - p1.y) * k)]
                }
            }
        }
    }

    // Подпись: при удалении в конце сменяется на «До встречи!»
    readonly property real bye: uninstall ? doneT : 0
    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        y: hero.height - 74 - height / 2 - 10 * hero.bye
        text: "Crumb"
        font.pixelSize: 26
        font.weight: Font.Bold
        color: Theme.onPrimaryContainer
        opacity: 1 - hero.bye
    }
    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        y: hero.height - 74 - height / 2 + 10 * (1 - hero.bye)
        visible: hero.bye > 0
        text: qsTr("До встречи!")
        font.pixelSize: 26
        font.weight: Font.Bold
        color: Theme.onPrimaryContainer
        opacity: hero.bye
    }
    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        y: hero.height - 46 - height / 2
        text: "Material You · Chromium · " + hero.version
        font.pixelSize: 13
        color: Qt.alpha(Theme.onPrimaryContainer, 0.75)
    }
}
