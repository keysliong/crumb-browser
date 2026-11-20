// Переключатель Material 3, у которого бегунок — печенька.
// При включении бегунок вырастает, переезжает с пружиной и делает оборот.

import QtQuick

Item {
    id: root

    property bool checked: false
    property string text: ""
    property string description: ""
    signal toggled(bool checked)

    implicitWidth: (labels.visible ? labels.implicitWidth + 16 : 0) + track.width
    implicitHeight: Math.max(36, labels.implicitHeight)
    opacity: enabled ? 1 : 0.38
    activeFocusOnTab: true

    Accessible.role: Accessible.CheckBox
    Accessible.checkable: true
    Accessible.checked: checked
    Accessible.name: text
    Keys.onSpacePressed: toggle()

    function toggle() {
        Prefs.click()
        checked = !checked
        toggled(checked)
    }

    Column {
        id: labels
        visible: root.text.length > 0
        anchors.left: parent.left
        anchors.right: track.left
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2
        CText { width: parent.width; text: root.text; size: 15; wrapMode: Text.WordWrap }
        CText {
            width: parent.width
            visible: root.description.length > 0
            text: root.description
            size: 12.5
            color: Theme.onSurfaceVariant
            wrapMode: Text.WordWrap
        }
    }

    Rectangle {
        id: track
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: 52
        height: 32
        radius: 16
        color: root.checked ? Theme.primary : Theme.surfaceContainerHighest
        border.width: root.checked ? 0 : 2
        border.color: Theme.outline
        Behavior on color { ColorAnimation { duration: Prefs.dur(200) } }

        CookieShape {
            id: thumb
            property real d: root.checked || tap.pressed ? 26 : 18
            width: d
            height: d
            waves: Math.max(6, Prefs.cookieWaves - 1)
            depth: Prefs.cookieDepth * 1.4
            x: root.checked ? track.width - width - 3 : 6 + (18 - d) / 2
            anchors.verticalCenter: parent.verticalCenter
            color: root.checked ? Theme.onPrimary : Theme.outline
            rotation: root.checked ? 180 : 0
            Behavior on x { enabled: Prefs.animations; SpringAnimation { spring: Prefs.spring(4); damping: 0.28; epsilon: 0.1 } }
            Behavior on d { enabled: Prefs.animations; SpringAnimation { spring: Prefs.spring(5); damping: 0.3; epsilon: 0.1 } }
            Behavior on rotation { enabled: Prefs.animations; SpringAnimation { spring: Prefs.spring(2.5); damping: 0.3; epsilon: 0.5 } }
            Behavior on color { ColorAnimation { duration: Prefs.dur(200) } }

            Text {
                anchors.centerIn: parent
                visible: root.checked
                text: ""
                rotation: -thumb.rotation
                font.family: Theme.iconFont
                font.pixelSize: 12
                color: Theme.primary
            }
        }
    }

    HoverHandler { cursorShape: Prefs.handCursor ? Qt.PointingHandCursor : Qt.ArrowCursor }
    TapHandler { id: tap; onTapped: root.toggle() }
}
