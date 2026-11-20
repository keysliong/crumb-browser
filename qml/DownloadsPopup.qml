// Компактное окно загрузок под кнопкой на панели (как в Edge/Chrome):
// последние файлы, прогресс, «Открыть файл», папка. Полный список — crumb://downloads.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Crumb
import Crumb.Ui

Popup {
    id: dp
    popupType: Popup.Window
    width: 400
    height: Math.min(560, header.implicitHeight + (Downloads.count === 0 ? 170 : Math.min(list.contentHeight, 440)) + 34)
    padding: 12

    signal openFullPage()

    function eta(rec, total, speed) {
        if (speed <= 0 || total <= 0) return ""
        const s = Math.round((total - rec) / speed)
        if (s < 60) return qsTr("%1 с").arg(s)
        if (s < 3600) return qsTr("%1 мин").arg(Math.round(s / 60))
        return qsTr("%1 ч").arg((s / 3600).toFixed(1))
    }

    background: Rectangle { color: Theme.surfaceContainer; radius: 24; border.color: Theme.outlineVariant }

    contentItem: ColumnLayout {
        spacing: 6

        // ---- заголовок
        RowLayout {
            id: header
            Layout.fillWidth: true
            Layout.leftMargin: 8
            spacing: 4
            CText { text: qsTr("Загрузки"); size: 17; font.weight: Font.DemiBold; Layout.fillWidth: true }
            CookieButton {
                size: 32; icon: ""; variant: "surface"
                toolTip: qsTr("Открыть папку загрузок")
                onClicked: { Downloads.openFolder(); dp.close() }
            }
            CookieButton {
                size: 32; icon: ""; variant: "surface"
                enabled: Downloads.count > Downloads.activeCount
                toolTip: qsTr("Очистить завершённые")
                onClicked: Downloads.clearFinished()
            }
            CookieButton {
                size: 32; icon: ""; variant: "surface"
                toolTip: qsTr("Все загрузки (%1)").arg(Settings.shortcutDownloads)
                onClicked: { dp.close(); dp.openFullPage() }
            }
        }

        // ---- пусто
        ColumnLayout {
            visible: Downloads.count === 0
            Layout.fillWidth: true
            Layout.topMargin: 14
            Layout.bottomMargin: 14
            spacing: 8
            CookieShape {
                Layout.alignment: Qt.AlignHCenter
                width: 64; height: 64
                color: Theme.surfaceContainerHigh
                CText {
                    anchors.centerIn: parent
                    text: ""; font.family: Theme.iconFont; size: 24
                    color: Theme.onSurfaceVariant
                }
            }
            CText { Layout.alignment: Qt.AlignHCenter; text: qsTr("Пока ничего не загружено"); size: 14.5; font.weight: Font.Medium }
            CText { Layout.alignment: Qt.AlignHCenter; text: qsTr("Скачанные файлы появятся здесь"); size: 12.5; color: Theme.onSurfaceVariant }
        }

        // ---- список
        ListView {
            id: list
            visible: Downloads.count > 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 2
            model: Downloads
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Rectangle {
                id: row
                required property int uid
                required property string name
                required property string path
                required property real received
                required property real total
                required property real speed
                required property int percent
                required property string status
                readonly property bool active: status === "progress" || status === "paused"
                readonly property bool hovered: rowHover.hovered
                width: list.width
                height: active ? 74 : 62
                radius: 16
                color: hovered ? Qt.alpha(Theme.onSurface, 0.07) : "transparent"

                HoverHandler { id: rowHover; cursorShape: row.status === "done" ? Qt.PointingHandCursor : Qt.ArrowCursor }
                TapHandler {
                    onTapped: if (row.status === "done") { Downloads.open(row.uid); dp.close() }
                }

                // Значок файла из Windows на подложке-печеньке
                CookieShape {
                    id: iconBox
                    x: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: 42; height: 42
                    color: row.active ? Theme.primaryContainer : Theme.surfaceContainerHigh
                    spin: row.status === "progress" && Prefs.animations ? 40 : 0
                    Image {
                        anchors.centerIn: parent
                        rotation: -parent.rotation
                        width: 28; height: 28
                        sourceSize: Qt.size(48, 48)
                        source: "image://fileicon/" + encodeURIComponent(row.path.length ? row.path : row.name)
                        opacity: row.status === "canceled" || row.status === "failed" ? 0.45 : 1
                        smooth: true
                        mipmap: true
                    }
                }

                ColumnLayout {
                    anchors.left: iconBox.right
                    anchors.leftMargin: 12
                    anchors.right: actions.left
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 3
                    CText {
                        Layout.fillWidth: true
                        text: row.name
                        size: 14
                        elide: Text.ElideMiddle
                        font.strikeout: row.status === "canceled"
                        color: row.status === "canceled" || row.status === "failed" ? Theme.onSurfaceVariant : Theme.onSurface
                    }
                    // Полоса прогресса
                    Item {
                        visible: row.active
                        Layout.fillWidth: true
                        Layout.preferredHeight: 5
                        Rectangle { anchors.fill: parent; radius: 2.5; color: Theme.surfaceContainerHighest }
                        Rectangle {
                            height: parent.height; radius: 2.5
                            color: row.status === "paused" ? Theme.outline : Theme.primary
                            width: parent.width * (row.percent >= 0 ? row.percent / 100 : 0.3)
                            Behavior on width { enabled: Prefs.animations; NumberAnimation { duration: 250 } }
                        }
                    }
                    CText {
                        Layout.fillWidth: true
                        size: 12.5
                        elide: Text.ElideRight
                        font.underline: row.status === "done" && row.hovered
                        color: row.status === "done" ? Theme.primary
                             : row.status === "failed" ? Theme.error : Theme.onSurfaceVariant
                        text: row.status === "done" ? qsTr("Открыть файл")
                            : row.status === "canceled" ? qsTr("Отменено")
                            : row.status === "failed" ? qsTr("Не удалось загрузить")
                            : row.status === "paused" ? qsTr("Пауза · %1 из %2").arg(Downloads.formatSize(row.received)).arg(Downloads.formatSize(row.total))
                            : [Downloads.formatSize(row.received) + (row.total > 0 ? qsTr(" из ") + Downloads.formatSize(row.total) : ""),
                               row.speed > 0 ? Downloads.formatSize(row.speed) + qsTr("/с") : "",
                               dp.eta(row.received, row.total, row.speed)].filter(x => x.length).join(" · ")
                    }
                }

                // Действия: у активных — всегда, у остальных — при наведении
                Row {
                    id: actions
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    CookieButton {
                        visible: row.status === "progress"
                        size: 30; icon: ""; variant: "surface"; toolTip: qsTr("Пауза")
                        onClicked: Downloads.pause(row.uid)
                    }
                    CookieButton {
                        visible: row.status === "paused"
                        size: 30; icon: ""; variant: "tonal"; toolTip: qsTr("Продолжить")
                        onClicked: Downloads.resume(row.uid)
                    }
                    CookieButton {
                        visible: row.active
                        size: 30; icon: ""; variant: "surface"; toolTip: qsTr("Отменить")
                        onClicked: Downloads.cancel(row.uid)
                    }
                    CookieButton {
                        visible: row.status === "done" && row.hovered
                        size: 30; icon: ""; variant: "surface"; toolTip: qsTr("Показать в папке")
                        onClicked: { Downloads.showInFolder(row.uid); dp.close() }
                    }
                    CookieButton {
                        visible: !row.active && row.hovered
                        size: 30; icon: ""; variant: "surface"; toolTip: qsTr("Убрать из списка")
                        onClicked: Downloads.remove(row.uid)
                    }
                }
            }
        }
    }
}
