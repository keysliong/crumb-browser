// Загрузки (crumb://downloads): прогресс, скорость, пауза/продолжить/отмена,
// открыть файл и показать в папке. История хранится между запусками.

import QtQuick
import QtQuick.Layouts
import Crumb
import Crumb.Ui

Item {
    id: page

    Rectangle { anchors.fill: parent; color: Theme.surface }

    function fileGlyph(name) {
        const ext = (name.split(".").pop() || "").toLowerCase()
        if (["jpg", "jpeg", "png", "gif", "webp", "bmp", "svg"].indexOf(ext) >= 0) return ""
        if (["mp4", "mkv", "avi", "mov", "webm"].indexOf(ext) >= 0) return ""
        if (["mp3", "wav", "flac", "ogg", "m4a"].indexOf(ext) >= 0) return ""
        if (["zip", "rar", "7z", "gz", "tar"].indexOf(ext) >= 0) return ""
        if (["exe", "msi"].indexOf(ext) >= 0) return ""
        if (["pdf", "doc", "docx", "txt", "rtf", "odt"].indexOf(ext) >= 0) return ""
        return ""
    }
    function eta(rec, total, speed) {
        if (speed <= 0 || total <= 0) return ""
        const s = Math.round((total - rec) / speed)
        if (s < 60) return qsTr("осталось %1 с").arg(s)
        if (s < 3600) return qsTr("осталось %1 мин").arg(Math.round(s / 60))
        return qsTr("осталось %1 ч").arg((s / 3600).toFixed(1))
    }

    ColumnLayout {
        id: header
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 28
        width: Math.min(parent.width - 64, 820)
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 14
            CookieShape {
                width: 48; height: 48
                color: Theme.primaryContainer
                Text {
                    anchors.centerIn: parent
                    text: ""
                    font.family: Theme.iconFont
                    font.pixelSize: 20
                    color: Theme.onPrimaryContainer
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                CText { text: qsTr("Загрузки"); size: 28; font.weight: Font.Bold }
                CText {
                    text: Downloads.activeCount > 0 ? qsTr("Загружается: %1").arg(Downloads.activeCount)
                                                    : qsTr("Папка: %1").arg(Settings.downloadsDir)
                    size: 13
                    color: Theme.onSurfaceVariant
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }
            }
            CookieButton { icon: ""; text: qsTr("Открыть папку"); onClicked: Downloads.openFolder() }
            CookieButton {
                icon: ""; text: qsTr("Очистить"); variant: "surface"
                enabled: Downloads.count > Downloads.activeCount
                onClicked: Downloads.clearFinished()
            }
        }
    }

    // Пусто
    ColumnLayout {
        anchors.centerIn: parent
        visible: Downloads.count === 0
        spacing: 12
        CookieShape {
            Layout.alignment: Qt.AlignHCenter
            width: 96; height: 96
            color: Theme.surfaceContainerHigh
            spin: Prefs.animations ? 12 : 0
            Text {
                anchors.centerIn: parent
                rotation: -parent.rotation
                text: ""
                font.family: Theme.iconFont
                font.pixelSize: 36
                color: Theme.onSurfaceVariant
            }
        }
        CText { Layout.alignment: Qt.AlignHCenter; text: qsTr("Пока ничего не загружено"); size: 18; font.weight: Font.Medium }
        CText { Layout.alignment: Qt.AlignHCenter; text: qsTr("Скачанные файлы появятся здесь"); color: Theme.onSurfaceVariant }
    }

    ListView {
        id: list
        anchors.top: header.bottom
        anchors.topMargin: 18
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: header.width
        clip: true
        spacing: 10
        model: Downloads
        boundsBehavior: Flickable.StopAtBounds
        add: Transition { NumberAnimation { properties: "opacity"; from: 0; to: 1; duration: Prefs.dur(250) } }
        displaced: Transition { NumberAnimation { properties: "y"; duration: Prefs.dur(250); easing.type: Easing.OutCubic } }

        delegate: Rectangle {
            id: card
            required property int uid
            required property string name
            required property string path
            required property string url
            required property real received
            required property real total
            required property real speed
            required property int percent
            required property string status
            readonly property bool active: status === "progress" || status === "paused"
            width: list.width
            height: 92
            radius: 24
            color: Theme.surfaceContainerLow

            RowLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 16

                CookieShape {
                    Layout.preferredWidth: 52
                    Layout.preferredHeight: 52
                    color: card.status === "done" ? Theme.secondaryContainer
                         : card.active ? Theme.primaryContainer : Theme.surfaceContainerHighest
                    spin: card.status === "progress" && Prefs.animations ? 60 : 0
                    Text {
                        anchors.centerIn: parent
                        rotation: -parent.rotation
                        text: page.fileGlyph(card.name)
                        font.family: Theme.iconFont
                        font.pixelSize: 20
                        color: card.status === "done" ? Theme.onSecondaryContainer : Theme.onPrimaryContainer
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    CText {
                        Layout.fillWidth: true
                        text: card.name
                        size: 15
                        font.weight: Font.Medium
                        elide: Text.ElideMiddle
                        font.strikeout: card.status === "canceled"
                        color: card.status === "canceled" || card.status === "failed" ? Theme.onSurfaceVariant : Theme.onSurface
                    }
                    CText {
                        Layout.fillWidth: true
                        size: 12.5
                        color: card.status === "failed" ? Theme.error : Theme.onSurfaceVariant
                        elide: Text.ElideRight
                        text: card.status === "done" ? qsTr("Готово · %1").arg(Downloads.formatSize(card.total > 0 ? card.total : card.received))
                            : card.status === "canceled" ? qsTr("Отменено")
                            : card.status === "failed" ? qsTr("Не удалось загрузить")
                            : card.status === "paused" ? qsTr("Пауза · %1 из %2").arg(Downloads.formatSize(card.received)).arg(Downloads.formatSize(card.total))
                            : [Downloads.formatSize(card.received) + (card.total > 0 ? qsTr(" из ") + Downloads.formatSize(card.total) : ""),
                               card.speed > 0 ? Downloads.formatSize(card.speed) + qsTr("/с") : "",
                               page.eta(card.received, card.total, card.speed)].filter(x => x.length).join(" · ")
                    }
                    // Полоса прогресса
                    Item {
                        visible: card.active
                        Layout.fillWidth: true
                        Layout.preferredHeight: 6
                        Rectangle { anchors.fill: parent; radius: 3; color: Theme.surfaceContainerHighest }
                        Rectangle {
                            height: parent.height
                            radius: 3
                            color: card.status === "paused" ? Theme.outline : Theme.primary
                            width: parent.width * (card.percent >= 0 ? card.percent / 100 : 0.3)
                            Behavior on width { enabled: Prefs.animations; SpringAnimation { spring: Prefs.spring(3); damping: 0.4 } }
                        }
                    }
                }

                Row {
                    spacing: 8
                    CookieButton {
                        visible: card.status === "progress"
                        size: 36; icon: ""; toolTip: qsTr("Пауза")
                        onClicked: Downloads.pause(card.uid)
                    }
                    CookieButton {
                        visible: card.status === "paused"
                        size: 36; icon: ""; variant: "filled"; toolTip: qsTr("Продолжить")
                        onClicked: Downloads.resume(card.uid)
                    }
                    CookieButton {
                        visible: card.active
                        size: 36; icon: ""; variant: "surface"; toolTip: qsTr("Отменить")
                        onClicked: Downloads.cancel(card.uid)
                    }
                    CookieButton {
                        visible: card.status === "done"
                        size: 36; icon: ""; variant: "filled"; toolTip: qsTr("Открыть")
                        onClicked: Downloads.open(card.uid)
                    }
                    CookieButton {
                        visible: card.status === "done"
                        size: 36; icon: ""; toolTip: qsTr("Показать в папке")
                        onClicked: Downloads.showInFolder(card.uid)
                    }
                    CookieButton {
                        visible: !card.active
                        size: 36; icon: ""; variant: "surface"; toolTip: qsTr("Убрать из списка")
                        onClicked: Downloads.remove(card.uid)
                    }
                }
            }
        }
    }
}
