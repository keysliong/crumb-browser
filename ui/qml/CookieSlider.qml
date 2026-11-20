// Ползунок Material 3: активная часть дорожки — основной цвет,
// бегунок — печенька, над ним при перетаскивании всплывает значение.

import QtQuick

Item {
    id: root

    property real from: 0
    property real to: 1
    property real value: 0
    property real stepSize: 0            // 0 — плавно
    property int decimals: stepSize >= 1 ? 0 : 2
    property string suffix: ""
    property var formatValue: null       // function(v) → строка подписи
    property bool showTrack: true        // false — дорожку рисует владелец (радуга оттенков)
    signal moved(real value)

    implicitWidth: 240
    implicitHeight: 36
    opacity: enabled ? 1 : 0.38

    readonly property real ratio: to > from ? Math.max(0, Math.min(1, (value - from) / (to - from))) : 0
    readonly property real thumbX: ratio * (width - thumb.width)

    function setFromX(x) {
        let v = from + Math.max(0, Math.min(1, (x - thumb.width / 2) / (width - thumb.width))) * (to - from)
        if (stepSize > 0) v = Math.round((v - from) / stepSize) * stepSize + from
        v = Math.max(from, Math.min(to, v))
        if (v !== value) {
            value = v
            moved(v)
        }
    }

    Rectangle {   // неактивная часть
        visible: root.showTrack
        anchors.verticalCenter: parent.verticalCenter
        x: root.thumbX + thumb.width / 2
        width: root.width - x
        height: 8
        radius: 4
        color: Theme.surfaceContainerHighest
    }
    Rectangle {   // активная часть
        visible: root.showTrack
        anchors.verticalCenter: parent.verticalCenter
        width: root.thumbX + thumb.width / 2
        height: 8
        radius: 4
        color: Theme.primary
    }

    CookieShape {
        id: thumb
        width: area.pressed ? 30 : 26
        height: width
        anchors.verticalCenter: parent.verticalCenter
        x: root.thumbX - (width - 26) / 2
        waves: Prefs.cookieWaves
        depth: Prefs.cookieDepth * 1.3
        color: Theme.primary
        rotation: root.ratio * 180
        Behavior on width { enabled: Prefs.animations; SpringAnimation { spring: Prefs.spring(5); damping: 0.3 } }
    }

    // Значение над бегунком
    Rectangle {
        visible: area.pressed
        width: valueText.implicitWidth + 16
        height: 26
        radius: 13
        color: Theme.inverseSurface
        x: thumb.x + thumb.width / 2 - width / 2
        y: -height - 2
        CText {
            id: valueText
            anchors.centerIn: parent
            size: 12
            color: Theme.inverseOnSurface
            text: root.formatValue ? root.formatValue(root.value) : root.value.toFixed(root.decimals) + root.suffix
        }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        cursorShape: Prefs.handCursor ? Qt.PointingHandCursor : Qt.ArrowCursor
        onPressed: (e) => root.setFromX(e.x)
        onPositionChanged: (e) => { if (pressed) root.setFromX(e.x) }
    }
}
