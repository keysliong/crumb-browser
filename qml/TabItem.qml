// Вкладка (горизонтальная — в заголовке окна, или вертикальная — в боковой
// полосе). Сама вкладка — «ярлычок» со скруглением; кнопка закрытия — печенька.
// Перед первой вкладкой группы рисуется метка группы.

import QtQuick
import Crumb.Ui

Item {
    id: root

    property string title: ""
    property string favicon: ""
    property string glyph: ""            // значок вместо иконки сайта (настройки)
    property bool loading: false
    property bool active: false
    property bool vertical: false
    property real tabWidth: 200           // для горизонтальных
    property real tabHeight: Settings.tabsHeight
    property real radius: Settings.tabsRadius
    property bool showIcon: Settings.tabsIcons

    // группа
    property bool inGroup: false
    property bool groupStart: false
    property string groupName: ""
    property color groupColor: "transparent"
    property bool groupCollapsed: false

    signal activated()
    signal closeRequested()
    signal contextMenuRequested()
    signal groupClicked()
    signal groupMenuRequested()

    readonly property bool tabShown: !(inGroup && groupCollapsed) || active
    readonly property real chipExtent: groupStart ? (vertical ? 32 : chip.implicitWidth + 6) : 0
    readonly property real targetWidth: vertical ? (parent ? parent.width : 220)
                                                 : chipExtent + (tabShown ? tabWidth : 0)
    readonly property real targetHeight: vertical ? chipExtent + (tabShown ? tabHeight : 0)
                                                  : Math.max(tabHeight, 32)

    // Появление с пружиной: вкладка «вырастает».
    property bool appeared: false
    Component.onCompleted: appeared = true
    width: vertical ? targetWidth : (appeared ? targetWidth : 0)
    height: vertical ? (appeared ? targetHeight : 0) : targetHeight
    Behavior on width { enabled: Prefs.animations && !root.vertical; SpringAnimation { spring: Prefs.spring(3.5); damping: 0.32; epsilon: 0.25 } }
    Behavior on height { enabled: Prefs.animations && root.vertical; SpringAnimation { spring: Prefs.spring(3.5); damping: 0.32; epsilon: 0.25 } }
    clip: true

    // ---------------------------------------------------- метка группы
    Rectangle {
        id: chip
        visible: root.groupStart
        x: root.vertical ? 8 : 2
        y: root.vertical ? 4 : (root.height - height) / 2
        height: 24
        radius: 12
        implicitWidth: Math.max(26, chipText.implicitWidth + 22)
        width: implicitWidth
        color: root.groupColor
        CText {
            id: chipText
            anchors.centerIn: parent
            text: root.groupName.length ? root.groupName : " "
            color: Theme.dark ? "#1b1b1f" : "#ffffff"
            size: 12
            font.weight: Font.DemiBold
        }
        HoverHandler { cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: root.groupClicked() }
        TapHandler { acceptedButtons: Qt.RightButton; onTapped: root.groupMenuRequested() }
    }

    // ---------------------------------------------------------- вкладка
    Item {
        id: tab
        visible: root.tabShown
        x: root.vertical ? 0 : root.chipExtent
        y: root.vertical ? root.chipExtent : 0
        width: root.vertical ? root.width : root.tabWidth
        height: root.vertical ? root.tabHeight : root.height

        Rectangle {
            id: bg
            anchors.fill: parent
            anchors.topMargin: root.vertical ? 2 : 4
            anchors.bottomMargin: root.vertical ? 2 : 0
            radius: root.radius
            color: root.active ? Theme.surface
                               : (hover.hovered ? Qt.alpha(Theme.onSurface, 0.08) : "transparent")
            Behavior on color { ColorAnimation { duration: Prefs.dur(160) } }
            // Нижние углы активной горизонтальной вкладки прямые — она «врастает» в панель.
            Rectangle {
                visible: root.active && !root.vertical
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: root.radius
                color: Theme.surface
            }
        }

        // Линия цвета группы: снизу (горизонтально) или слева (вертикально)
        Rectangle {
            visible: root.inGroup
            x: root.vertical ? 3 : 8
            y: root.vertical ? 8 : parent.height - 3
            width: root.vertical ? 3 : parent.width - 16
            height: root.vertical ? parent.height - 16 : 3
            radius: 1.5
            color: root.groupColor
        }

        Item {
            id: iconBox
            visible: root.showIcon
            x: 12
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: root.vertical ? 0 : 2
            width: root.showIcon ? 18 : 0
            height: 18

            CookieShape {
                anchors.fill: parent
                visible: root.loading
                waves: 8
                depth: 0.18
                color: Theme.primary
                spin: Prefs.animations ? 220 : 0
            }
            Image {
                anchors.fill: parent
                visible: !root.loading && root.favicon.length > 0
                source: root.favicon
                sourceSize: Qt.size(36, 36)
                smooth: true
                mipmap: true
            }
            Text {
                anchors.centerIn: parent
                visible: !root.loading && root.favicon.length === 0
                text: root.glyph.length ? root.glyph : ""
                font.family: Theme.iconFont
                font.pixelSize: 14
                color: Theme.onSurfaceVariant
            }
        }

        CText {
            anchors.left: iconBox.right
            anchors.leftMargin: root.showIcon ? 10 : 2
            anchors.right: closeBtn.visible ? closeBtn.left : parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: root.vertical ? 0 : 2
            visible: root.vertical || root.tabWidth > 70
            text: root.title.length ? root.title : qsTr("Новая вкладка")
            elide: Text.ElideRight
            color: root.active ? Theme.onSurface : Theme.onSurfaceVariant
            size: 13
            font.weight: root.active ? Font.Medium : Font.Normal
        }

        CookieButton {
            id: closeBtn
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: root.vertical ? 0 : 2
            visible: root.active || hover.hovered || (!root.vertical && root.tabWidth > 110)
            size: 20
            variant: root.active ? "tonal" : "surface"
            icon: ""
            toolTip: qsTr("Закрыть вкладку")
            onClicked: root.closeRequested()
        }

        HoverHandler { id: hover }
        TapHandler {
            acceptedButtons: Qt.LeftButton
            onPressedChanged: if (pressed) root.activated()
        }
        TapHandler { acceptedButtons: Qt.MiddleButton; onTapped: root.closeRequested() }
        TapHandler { acceptedButtons: Qt.RightButton; onTapped: root.contextMenuRequested() }
    }
}
