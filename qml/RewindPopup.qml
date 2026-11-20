// Панель Rewind: машина времени текущей страницы, подписки на изменения и
// поиск по всему, что вы видели. Открывается кнопкой на панели (Ctrl+Shift+Y).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Crumb
import Crumb.Ui

CookiePopup {
    id: rw
    width: 600
    height: Math.min(640, col.implicitHeight + 28)
    padding: 14

    property string pageUrl: ""          // реальный адрес текущей вкладки
    property string pageTitle: ""
    property var snapshots: []
    property string query: ""
    property var results: []

    signal openUrl(string url)           // открыть адрес новой вкладкой
    signal pickFragment()                // запустить выбор фрагмента в странице

    function reload() {
        snapshots = pageUrl.length ? Rewind.snapshotsFor(pageUrl) : []
        results = query.length ? Rewind.search(query) : []
    }
    onOpened: { reload(); Rewind.markWatchesSeen(); timeline.positionViewAtEnd() }
    Connections {
        target: Rewind
        function onSnapshotsChanged() { if (rw.opened) rw.reload() }
    }

    background: Rectangle { color: Theme.surfaceContainer; radius: 26; border.color: Theme.outlineVariant }

    contentItem: Flickable {
        clip: true
        contentHeight: col.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        ColumnLayout {
            id: col
            width: parent.width
            spacing: 10

            // ---- заголовок
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                CookieShape {
                    width: 40; height: 40
                    gradientColors: [Theme.primary, Theme.tertiary]
                    CText { anchors.centerIn: parent; text: ""; font.family: "Segoe Fluent Icons, Segoe MDL2 Assets"; size: 18; color: Theme.onPrimary }
                }
                ColumnLayout {
                    spacing: 0
                    Layout.fillWidth: true
                    CText { text: qsTr("Rewind"); size: 18; font.weight: Font.DemiBold }
                    CText {
                        text: Settings.rewindEnabled
                              ? qsTr("%1 снимков · %2 МБ · только на этом компьютере").arg(Rewind.count).arg(Rewind.sizeMB.toFixed(1))
                              : qsTr("Снимки выключены в настройках")
                        size: 12.5; color: Theme.onSurfaceVariant
                    }
                }
                CookieSwitch {
                    checked: Settings.rewindEnabled
                    onToggled: (on) => Settings.rewindEnabled = on
                }
            }

            // ---- машина времени этой страницы
            CText {
                text: rw.pageUrl.length ? qsTr("Эта страница в прошлом") : qsTr("Откройте сайт, чтобы увидеть его историю")
                size: 14; font.weight: Font.DemiBold; topPadding: 4
            }
            CText {
                visible: rw.pageUrl.length > 0 && rw.snapshots.length === 0
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Пока нет снимков. Crumb сохраняет страницу после каждой загрузки, если её текст изменился "
                         + "(кроме страниц с паролями и сайтов из чёрного списка).")
                size: 13; color: Theme.onSurfaceVariant
            }
            ListView {
                id: timeline
                visible: rw.snapshots.length > 0
                Layout.fillWidth: true
                Layout.preferredHeight: 176
                orientation: ListView.Horizontal
                spacing: 10
                clip: true
                model: rw.snapshots
                ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
                delegate: Rectangle {
                    id: card
                    required property var modelData
                    required property int index
                    readonly property bool latest: index === rw.snapshots.length - 1
                    width: 168; height: 164
                    radius: 18
                    color: cardHover.hovered ? Theme.surfaceContainerHighest : Theme.surfaceContainerHigh
                    border.color: latest ? Theme.primary : "transparent"
                    border.width: 2
                    Rectangle {
                        id: thumbBox
                        x: 6; y: 6; width: parent.width - 12; height: 92
                        radius: 13; clip: true
                        color: Theme.surfaceContainerLowest
                        Image {
                            anchors.fill: parent
                            source: card.modelData.thumb
                            fillMode: Image.PreserveAspectCrop
                            verticalAlignment: Image.AlignTop
                            asynchronous: true
                        }
                        CText {
                            visible: !card.modelData.thumb
                            anchors.centerIn: parent
                            text: ""; font.family: "Segoe Fluent Icons, Segoe MDL2 Assets"; size: 24
                            color: Theme.onSurfaceVariant
                        }
                    }
                    CText {
                        x: 10; y: 102; width: parent.width - 20
                        text: card.latest ? qsTr("Сейчас · ") + card.modelData.date : card.modelData.date
                        size: 12.5; font.weight: Font.DemiBold; elide: Text.ElideRight
                    }
                    HoverHandler { id: cardHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: { rw.openUrl(Rewind.pageUrl(card.modelData.id)); rw.close() } }
                    Row {
                        x: 6; y: 124; spacing: 4
                        CookieButton {
                            size: 28; icon: ""; variant: "surface"
                            toolTip: qsTr("Открыть эту версию")
                            onClicked: { rw.openUrl(Rewind.pageUrl(card.modelData.id)); rw.close() }
                        }
                        CookieButton {
                            size: 28; icon: ""; variant: "tonal"
                            visible: rw.snapshots.length > 1
                            text: card.latest ? qsTr("Что нового") : qsTr("Что изменилось")
                            toolTip: card.latest ? qsTr("Сравнить с предыдущим снимком") : qsTr("Сравнить с текущей версией")
                            onClicked: { rw.openUrl(Rewind.diffUrl(card.modelData.id, 0)); rw.close() }
                        }
                    }
                }
            }

            // ---- подписки
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 4
                CText { text: qsTr("Слежу за изменениями"); size: 14; font.weight: Font.DemiBold; Layout.fillWidth: true }
                CookieButton {
                    visible: Rewind.watches.length > 0
                    size: 30; icon: ""; variant: "surface"
                    toolTip: qsTr("Проверить сейчас")
                    onClicked: Rewind.checkWatchesNow()
                }
                CookieButton {
                    size: 32; icon: ""; variant: "filled"
                    enabled: rw.pageUrl.length > 0
                    text: qsTr("Следить за фрагментом")
                    toolTip: qsTr("Выберите на странице цену, расписание, статус заказа — Crumb сообщит, когда он изменится")
                    onClicked: { rw.close(); rw.pickFragment() }
                }
            }
            CText {
                visible: Rewind.watches.length === 0
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Нажмите «Следить за фрагментом» и укажите часть страницы. Crumb будет проверять её каждые 30 минут "
                         + "в фоне и пришлёт уведомление, когда она изменится.")
                size: 13; color: Theme.onSurfaceVariant
            }
            Repeater {
                model: Rewind.watches
                Rectangle {
                    id: wrow
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: wcol.implicitHeight + 16
                    radius: 16
                    color: modelData.unseen ? Qt.alpha(Theme.primary, 0.14) : Theme.surfaceContainerHigh
                    ColumnLayout {
                        id: wcol
                        x: 14; y: 8
                        width: parent.width - 70
                        spacing: 2
                        CText { text: wrow.modelData.label; size: 13.5; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }
                        CText {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            size: 12; color: Theme.onSurfaceVariant
                            text: {
                                const host = wrow.modelData.url.replace(/^https?:\/\//, "").split("/")[0]
                                return wrow.modelData.changedAt
                                    ? qsTr("%1 · изменилось %2").arg(host).arg(Rewind.formatDate(wrow.modelData.changedAt))
                                    : qsTr("%1 · проверено %2").arg(host).arg(Rewind.formatDate(wrow.modelData.lastCheck))
                            }
                        }
                        CText {
                            visible: !!wrow.modelData.prevText && wrow.modelData.changedAt > 0
                            Layout.fillWidth: true
                            size: 12; color: Theme.primary; elide: Text.ElideRight
                            text: qsTr("было: %1 → стало: %2")
                                  .arg(String(wrow.modelData.prevText || "").replace(/\s+/g, " ").slice(0, 40))
                                  .arg(String(wrow.modelData.text || "").replace(/\s+/g, " ").slice(0, 40))
                        }
                    }
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: { rw.openUrl(wrow.modelData.url); rw.close() } }
                    CookieButton {
                        anchors.right: parent.right; anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        size: 30; icon: ""; variant: "surface"
                        toolTip: qsTr("Перестать следить")
                        onClicked: Rewind.removeWatch(wrow.modelData.id)
                    }
                }
            }

            // ---- поиск по прошлому
            CText { text: qsTr("Найти в прошлом"); size: 14; font.weight: Font.DemiBold; topPadding: 6 }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 42
                radius: 21
                color: Theme.surfaceContainerHighest
                CText {
                    x: 16; anchors.verticalCenter: parent.verticalCenter
                    text: ""; font.family: "Segoe Fluent Icons, Segoe MDL2 Assets"; size: 15; color: Theme.onSurfaceVariant
                }
                TextInput {
                    id: searchInput
                    x: 42; width: parent.width - 56
                    anchors.verticalCenter: parent.verticalCenter
                    color: Theme.onSurface
                    selectionColor: Theme.primary
                    selectedTextColor: Theme.onPrimary
                    font.pixelSize: 14
                    clip: true
                    onTextEdited: searchTimer.restart()
                    CText {
                        visible: !searchInput.text.length
                        text: qsTr("Слово или фраза с любой страницы, которую вы открывали…")
                        color: Theme.onSurfaceVariant; size: 14
                    }
                }
                Timer { id: searchTimer; interval: 220; onTriggered: { rw.query = searchInput.text.trim(); rw.reload() } }
            }
            CText {
                visible: rw.query.length > 0 && rw.results.length === 0
                text: qsTr("Ничего не найдено")
                size: 13; color: Theme.onSurfaceVariant
            }
            Repeater {
                model: rw.results
                Rectangle {
                    id: res
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: 66
                    radius: 16
                    color: resHover.hovered ? Theme.surfaceContainerHighest : "transparent"
                    Rectangle {
                        x: 8; anchors.verticalCenter: parent.verticalCenter
                        width: 80; height: 50; radius: 10; clip: true
                        color: Theme.surfaceContainerLowest
                        Image { anchors.fill: parent; source: res.modelData.thumb; fillMode: Image.PreserveAspectCrop; verticalAlignment: Image.AlignTop; asynchronous: true }
                    }
                    ColumnLayout {
                        x: 100; width: parent.width - 110
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 1
                        CText { text: res.modelData.title || res.modelData.url; size: 13.5; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }
                        CText {
                            Layout.fillWidth: true
                            textFormat: Text.StyledText
                            text: String(res.modelData.snippet || "").replace(/[\r\n]+/g, " ")
                            size: 12; color: Theme.onSurfaceVariant; elide: Text.ElideRight
                        }
                        CText { text: res.modelData.date; size: 11.5; color: Theme.primary }
                    }
                    HoverHandler { id: resHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: { rw.openUrl(Rewind.pageUrl(res.modelData.id)); rw.close() } }
                }
            }
            Item { implicitHeight: 2 }
        }
    }
}
