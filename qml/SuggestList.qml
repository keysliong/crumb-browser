// Подсказки адресной строки: «искать …», прямой адрес и сайты из истории.
// Клавиатура остаётся в строке: ↑/↓ двигают выделение, Enter — открыть.
import QtQuick
import QtQuick.Layouts
import Crumb
import Crumb.Ui

FloatingPanel {
    id: sl

    property var items: []          // { kind: search|go|history, text, url, title, icon }
    property int current: -1
    readonly property int rowHeight: 44
    signal picked(var item)

    function move(step) {
        if (!items.length) return
        current = current + step < -1 ? items.length - 1 : (current + step >= items.length ? -1 : current + step)
    }

    Column {
        anchors.fill: parent
        spacing: 0
        Repeater {
            model: sl.items
            Rectangle {
                id: row
                required property var modelData
                required property int index
                readonly property bool sel: index === sl.current
                width: parent.width
                height: sl.rowHeight
                radius: 16
                color: sel ? Theme.secondaryContainer : (hov.hovered ? Qt.alpha(Theme.onSurface, 0.07) : "transparent")
                Behavior on color { ColorAnimation { duration: Prefs.dur(90) } }
                HoverHandler { id: hov; onHoveredChanged: if (hovered) sl.current = row.index }
                TapHandler { onTapped: sl.picked(row.modelData) }

                CookieShape {
                    id: ico
                    x: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: 30; height: 30
                    color: row.sel ? Theme.primary : Theme.surfaceContainerHighest
                    Image {
                        anchors.centerIn: parent
                        visible: row.modelData.kind === "history" && row.modelData.icon.length > 0
                        width: 16; height: 16
                        source: visible ? row.modelData.icon : ""
                        sourceSize: Qt.size(32, 32)
                        mipmap: true
                    }
                    Text {
                        anchors.centerIn: parent
                        visible: !(row.modelData.kind === "history" && row.modelData.icon.length > 0)
                        text: row.modelData.kind === "search" ? "" : row.modelData.kind === "go" ? "" : ""
                        font.family: Theme.iconFont
                        font.pixelSize: 13
                        color: row.sel ? Theme.onPrimary : Theme.onSurfaceVariant
                    }
                }
                RowLayout {
                    anchors.left: ico.right
                    anchors.leftMargin: 12
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8
                    CText {
                        text: row.modelData.title
                        size: 14
                        elide: Text.ElideRight
                        color: row.sel ? Theme.onSecondaryContainer : Theme.onSurface
                        Layout.maximumWidth: parent.width * 0.62
                    }
                    CText {
                        visible: row.modelData.text.length > 0
                        text: "— " + row.modelData.text
                        size: 12.5
                        elide: Text.ElideMiddle
                        color: row.sel ? Theme.onSecondaryContainer : Theme.primary
                        Layout.fillWidth: true
                    }
                    Item { Layout.fillWidth: true; visible: row.modelData.text.length === 0 }
                }
            }
        }
    }
}
