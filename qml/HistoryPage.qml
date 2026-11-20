// История (crumb://history, Ctrl+H): посещения по дням, поиск, удаление,
// недавно закрытые вкладки. Всё хранится только на этом компьютере.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Crumb
import Crumb.Ui

Item {
    id: page

    property var closedTabs: []
    signal openUrl(string url)
    signal reopenClosed(int index)

    Rectangle { anchors.fill: parent; color: Theme.surface }

    ListModel { id: model }
    property bool more: false
    property double lastTs: 0

    function reload() {
        model.clear()
        lastTs = 0
        appendPage()
    }
    function appendPage() {
        const rows = History.query(search.text, 300, lastTs)
        for (const r of rows) model.append(r)
        more = rows.length === 300
        if (rows.length) lastTs = rows[rows.length - 1].ts
    }
    Component.onCompleted: reload()
    Connections { target: History; function onChanged() { refresh.restart() } }
    Timer { id: refresh; interval: 250; onTriggered: page.reload() }

    ColumnLayout {
        id: header
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 28
        width: Math.min(parent.width - 64, 860)
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 14
            CookieShape {
                width: 48; height: 48
                color: Theme.primaryContainer
                Text { anchors.centerIn: parent; text: ""; font.family: Theme.iconFont; font.pixelSize: 20; color: Theme.onPrimaryContainer }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                CText { text: qsTr("История"); size: 28; font.weight: Font.Bold }
                CText {
                    text: Settings.historyEnabled ? qsTr("Посещений: %1 · хранится полгода, только на этом компьютере").arg(History.count)
                                                  : qsTr("Запись истории выключена в настройках")
                    size: 13; color: Theme.onSurfaceVariant
                }
            }
            CookieButton {
                id: clearBtn
                icon: ""; text: qsTr("Очистить…"); variant: "surface"
                enabled: History.count > 0
                onClicked: clearMenu.popup(clearBtn, 0, clearBtn.height + 6)
            }
        }

        // Поиск
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            radius: 24
            color: Theme.surfaceContainerHigh
            border.width: search.activeFocus ? 2 : 0
            border.color: Theme.primary
            Text {
                id: g
                anchors.left: parent.left; anchors.leftMargin: 18
                anchors.verticalCenter: parent.verticalCenter
                text: ""; font.family: Theme.iconFont; font.pixelSize: 15; color: Theme.onSurfaceVariant
            }
            TextInput {
                id: search
                anchors.left: g.right; anchors.leftMargin: 12
                anchors.right: clearSearch.left; anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                font.family: Prefs.font; font.pixelSize: Prefs.px(15)
                color: Theme.onSurface
                selectByMouse: true
                selectionColor: Theme.primaryContainer
                clip: true
                onTextChanged: refresh.restart()
                CText {
                    anchors.fill: parent; verticalAlignment: Text.AlignVCenter
                    visible: search.text.length === 0
                    text: qsTr("Искать в истории — по названию или адресу")
                    color: Theme.onSurfaceVariant
                }
            }
            CookieButton {
                id: clearSearch
                anchors.right: parent.right; anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                visible: search.text.length > 0
                size: 32; variant: "surface"; icon: ""
                onClicked: search.text = ""
            }
        }

        // Недавно закрытые вкладки
        ColumnLayout {
            visible: page.closedTabs.length > 0 && search.text.length === 0
            Layout.fillWidth: true
            spacing: 8
            CText { text: qsTr("Недавно закрытые вкладки"); size: 14; font.weight: Font.DemiBold; color: Theme.onSurfaceVariant }
            Flow {
                Layout.fillWidth: true
                spacing: 8
                Repeater {
                    model: page.closedTabs.slice(-8).reverse()
                    Rectangle {
                        id: chip
                        required property var modelData
                        required property int index
                        height: 36
                        width: Math.min(240, chipRow.implicitWidth + 24)
                        radius: 18
                        color: chipHover.hovered ? Theme.secondaryContainer : Theme.surfaceContainerHigh
                        Behavior on color { ColorAnimation { duration: Prefs.dur(120) } }
                        HoverHandler { id: chipHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: page.reopenClosed(page.closedTabs.length - 1 - chip.index) }
                        RowLayout {
                            id: chipRow
                            anchors.fill: parent
                            anchors.leftMargin: 12; anchors.rightMargin: 12
                            spacing: 8
                            Image {
                                readonly property string ic: chip.modelData.url ? History.iconFor(chip.modelData.url) : ""
                                visible: ic.length > 0
                                source: ic
                                Layout.preferredWidth: 16; Layout.preferredHeight: 16
                                sourceSize: Qt.size(32, 32)
                            }
                            CText {
                                text: chip.modelData.title || chip.modelData.url || qsTr("Вкладка")
                                size: 13
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                Layout.maximumWidth: 190
                            }
                        }
                    }
                }
            }
        }
    }

    // Пусто
    ColumnLayout {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: 60
        visible: model.count === 0
        spacing: 12
        CookieShape {
            Layout.alignment: Qt.AlignHCenter
            width: 96; height: 96
            color: Theme.surfaceContainerHigh
            spin: Prefs.animations ? 12 : 0
            Text { anchors.centerIn: parent; rotation: -parent.rotation; text: ""; font.family: Theme.iconFont; font.pixelSize: 36; color: Theme.onSurfaceVariant }
        }
        CText {
            Layout.alignment: Qt.AlignHCenter
            text: search.text.length ? qsTr("Ничего не нашлось") : qsTr("История пока пуста")
            size: 18; font.weight: Font.Medium
        }
        CText {
            Layout.alignment: Qt.AlignHCenter
            text: search.text.length ? qsTr("Попробуйте другое слово") : qsTr("Сайты, которые вы открываете, появятся здесь")
            color: Theme.onSurfaceVariant
        }
    }

    ListView {
        id: list
        anchors.top: header.bottom
        anchors.topMargin: 14
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: header.width
        clip: true
        model: model
        spacing: 2
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        onAtYEndChanged: if (atYEnd && page.more) page.appendPage()
        displaced: Transition { NumberAnimation { properties: "y"; duration: Prefs.dur(220); easing.type: Easing.OutCubic } }
        remove: Transition { NumberAnimation { property: "opacity"; to: 0; duration: Prefs.dur(160) } }

        section.property: "day"
        section.delegate: Item {
            required property string section
            width: list.width
            height: 48
            CText {
                anchors.left: parent.left; anchors.leftMargin: 8
                anchors.bottom: parent.bottom; anchors.bottomMargin: 8
                text: parent.section
                size: 15; font.weight: Font.DemiBold
                color: Theme.primary
            }
        }

        delegate: Rectangle {
            id: row
            required property int index
            required property int vid
            required property string url
            required property string title
            required property string host
            required property string time
            required property string icon
            width: list.width
            height: 52
            radius: 16
            color: hov.hovered ? Qt.alpha(Theme.onSurface, 0.06) : "transparent"
            Behavior on color { ColorAnimation { duration: Prefs.dur(90) } }
            HoverHandler { id: hov; cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: page.openUrl(row.url) }

            CText {
                id: timeText
                x: 10
                width: 46
                anchors.verticalCenter: parent.verticalCenter
                text: row.time
                size: 12.5
                color: Theme.onSurfaceVariant
            }
            CookieShape {
                id: ic
                anchors.left: timeText.right
                anchors.verticalCenter: parent.verticalCenter
                width: 34; height: 34
                color: Theme.surfaceContainerHighest
                Image {
                    anchors.centerIn: parent
                    visible: row.icon.length > 0
                    width: 18; height: 18
                    source: row.icon
                    sourceSize: Qt.size(36, 36)
                    mipmap: true
                }
                CText {
                    anchors.centerIn: parent
                    visible: row.icon.length === 0
                    text: (row.host || "?")[0].toUpperCase()
                    size: 14; font.weight: Font.DemiBold
                    color: Theme.onSurfaceVariant
                }
            }
            RowLayout {
                anchors.left: ic.right; anchors.leftMargin: 12
                anchors.right: actions.left; anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                spacing: 10
                CText { text: row.title; size: 14; elide: Text.ElideRight; Layout.maximumWidth: parent.width * 0.7 }
                CText { text: row.host; size: 12.5; color: Theme.onSurfaceVariant; elide: Text.ElideRight; Layout.fillWidth: true }
            }
            Row {
                id: actions
                anchors.right: parent.right; anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                opacity: hov.hovered ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: Prefs.dur(120) } }
                CookieButton {
                    size: 30; variant: "surface"; icon: ""
                    toolTip: qsTr("Ещё с сайта %1").arg(row.host)
                    onClicked: search.text = row.host
                }
                CookieButton {
                    size: 30; variant: "surface"; icon: ""
                    toolTip: qsTr("Копировать адрес")
                    onClicked: Passwords.copyText(row.url)
                }
                CookieButton {
                    size: 30; variant: "surface"; icon: ""
                    toolTip: qsTr("Удалить из истории")
                    onClicked: { model.remove(row.index); History.remove(row.vid) }
                }
            }
        }
    }

    ThemedMenu {
        id: clearMenu
        ThemedMenuItem { text: qsTr("За последний час"); onTriggered: History.clear(60) }
        ThemedMenuItem { text: qsTr("За сегодня (24 часа)"); onTriggered: History.clear(24 * 60) }
        ThemedMenuItem { text: qsTr("За неделю"); onTriggered: History.clear(7 * 24 * 60) }
        ThemedMenuSeparator {}
        ThemedMenuItem { text: qsTr("Всю историю"); onTriggered: History.clear(0) }
    }
}
