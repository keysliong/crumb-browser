// Расширения (crumb://extensions): установка из Chrome Web Store, распакованные
// расширения, включение/выключение, разрешения, закрепление на панели, удаление.

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Crumb
import Crumb.Ui

Item {
    id: page

    signal openUrl(string url)

    Rectangle { anchors.fill: parent; color: Theme.surface }

    function isPinned(id) { return (Settings.extPinned || []).indexOf(id) >= 0 }
    function setPinned(id, on) {
        const list = (Settings.extPinned || []).filter(x => x !== id)
        if (on) list.push(id)
        Settings.extPinned = list
    }

    Flickable {
        anchors.fill: parent
        contentHeight: col.implicitHeight + 60
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: col
            width: Math.min(parent.width - 64, 860)
            x: (parent.width - width) / 2
            y: 28
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                spacing: 14
                CookieShape {
                    width: 48; height: 48
                    color: Theme.primaryContainer
                    Text {
                        anchors.centerIn: parent
                        text: ""
                        font.family: Theme.iconFont
                        font.pixelSize: 20
                        color: Theme.onPrimaryContainer
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    CText { text: qsTr("Расширения"); size: 28; font.weight: Font.Bold }
                    CText {
                        text: Extensions.ready ? qsTr("Установлено: %1").arg(Extensions.items.length) : qsTr("Подключаемся к Chromium…")
                        size: 13
                        color: Theme.onSurfaceVariant
                    }
                }
            }

            // Действия
            Flow {
                Layout.fillWidth: true
                spacing: 18
                CookieButton {
                    variant: "filled"
                    iconSource: "https://ssl.gstatic.com/chrome/webstore/images/icon_96px.png"
                    letter: "W"
                    text: qsTr("Chrome Web Store")
                    toolTip: qsTr("Откройте расширение в магазине и нажмите «Установить»")
                    onClicked: page.openUrl("https://chromewebstore.google.com/category/extensions")
                }
                CookieButton {
                    icon: ""
                    text: qsTr("Загрузить распакованное…")
                    toolTip: qsTr("Папка с manifest.json")
                    enabled: Extensions.ready
                    onClicked: Extensions.loadUnpacked()
                }
                CookieButton {
                    icon: ""; variant: "surface"
                    text: qsTr("Обновить список")
                    enabled: Extensions.ready
                    onClicked: Extensions.refresh()
                }
                CookieButton {
                    icon: ""; variant: "surface"
                    text: qsTr("chrome://extensions")
                    toolTip: qsTr("Полная страница расширений Chromium")
                    onClicked: page.openUrl("chrome://extensions/")
                }
            }

            // Ошибка последней операции
            Rectangle {
                id: errorBox
                visible: false
                Layout.fillWidth: true
                implicitHeight: errText.implicitHeight + 24
                radius: 16
                color: Theme.errorContainer
                CText { id: errText; anchors.fill: parent; anchors.margins: 12; wrapMode: Text.WordWrap; color: Theme.onErrorContainer }
                Connections {
                    target: Extensions
                    function onErrorOccurred(msg) { errText.text = msg; errorBox.visible = true; hideErr.restart() }
                }
                Timer { id: hideErr; interval: 8000; onTriggered: errorBox.visible = false }
            }

            // Пусто
            ColumnLayout {
                visible: Extensions.ready && Extensions.items.length === 0
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 40
                spacing: 10
                CookieShape {
                    Layout.alignment: Qt.AlignHCenter
                    width: 96; height: 96
                    color: Theme.surfaceContainerHigh
                    Text { anchors.centerIn: parent; text: ""; font.family: Theme.iconFont; font.pixelSize: 36; color: Theme.onSurfaceVariant }
                }
                CText { Layout.alignment: Qt.AlignHCenter; text: qsTr("Расширений пока нет"); size: 18; font.weight: Font.Medium }
                CText { Layout.alignment: Qt.AlignHCenter; text: qsTr("Найдите что-нибудь в Chrome Web Store"); color: Theme.onSurfaceVariant }
            }

            // Карточки расширений
            Repeater {
                model: Extensions.items
                Rectangle {
                    id: card
                    required property var modelData
                    property bool expanded: false
                    Layout.fillWidth: true
                    implicitHeight: cardCol.implicitHeight + 32
                    radius: 24
                    color: Theme.surfaceContainerLow
                    opacity: modelData.enabled ? 1 : 0.75
                    Behavior on implicitHeight { enabled: Prefs.animations; NumberAnimation { duration: Prefs.dur(220); easing.type: Easing.OutCubic } }

                    ColumnLayout {
                        id: cardCol
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 16
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 14
                            // Значок расширения в печеньке
                            CookieShape {
                                Layout.preferredWidth: 56
                                Layout.preferredHeight: 56
                                color: Theme.surfaceContainerHighest
                                Image {
                                    anchors.centerIn: parent
                                    width: 30; height: 30
                                    source: card.modelData.icon
                                    sourceSize: Qt.size(64, 64)
                                    smooth: true
                                    mipmap: true
                                    visible: status === Image.Ready
                                }
                                CText {
                                    anchors.centerIn: parent
                                    visible: !card.modelData.icon
                                    text: (card.modelData.name || "?")[0]
                                    size: 22
                                    font.weight: Font.DemiBold
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3
                                RowLayout {
                                    spacing: 8
                                    CText { text: card.modelData.name; size: 16; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.maximumWidth: 360 }
                                    CText { text: card.modelData.version; size: 12; color: Theme.onSurfaceVariant }
                                    Rectangle {
                                        radius: 8
                                        color: card.modelData.fromStore ? Theme.secondaryContainer : Theme.tertiaryContainer
                                        implicitWidth: badge.implicitWidth + 14
                                        implicitHeight: 20
                                        CText {
                                            id: badge
                                            anchors.centerIn: parent
                                            size: 11
                                            text: card.modelData.fromStore ? qsTr("Web Store") : (card.modelData.location === "UNPACKED" ? qsTr("Распакованное") : qsTr("Другое"))
                                            color: card.modelData.fromStore ? Theme.onSecondaryContainer : Theme.onTertiaryContainer
                                        }
                                    }
                                }
                                CText {
                                    Layout.fillWidth: true
                                    text: card.modelData.description
                                    size: 13
                                    color: Theme.onSurfaceVariant
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: card.expanded ? 6 : 2
                                    elide: Text.ElideRight
                                }
                            }
                            CookieSwitch {
                                checked: card.modelData.enabled
                                enabled: card.modelData.mayModify !== false
                                onToggled: (on) => Extensions.setEnabled(card.modelData.id, on)
                            }
                        }

                        // Кнопки карточки
                        Flow {
                            Layout.fillWidth: true
                            spacing: 14
                            CookieButton {
                                size: 34
                                icon: card.expanded ? "" : ""
                                variant: "surface"
                                text: card.expanded ? qsTr("Скрыть подробности") : qsTr("Разрешения и подробности")
                                onClicked: card.expanded = !card.expanded
                            }
                            CookieButton {
                                size: 34
                                visible: card.modelData.hasAction === true
                                icon: ""
                                checked: page.isPinned(card.modelData.id)
                                text: page.isPinned(card.modelData.id) ? qsTr("На панели") : qsTr("Закрепить на панели")
                                onClicked: page.setPinned(card.modelData.id, !page.isPinned(card.modelData.id))
                            }
                            CookieButton {
                                size: 34
                                visible: card.modelData.optionsUrl.length > 0
                                icon: ""
                                text: qsTr("Параметры")
                                onClicked: page.openUrl(card.modelData.optionsUrl)
                            }
                            CookieButton {
                                size: 34
                                visible: card.modelData.mayModify !== false
                                icon: ""
                                color: Theme.errorContainer
                                contentColor: Theme.onErrorContainer
                                text: qsTr("Удалить")
                                onClicked: { removePopup.ext = card.modelData; removePopup.open() }
                            }
                        }

                        // Подробности: разрешения, доступ к сайтам, ID, путь
                        ColumnLayout {
                            visible: card.expanded
                            Layout.fillWidth: true
                            spacing: 6
                            CText { text: qsTr("Разрешения"); size: 13; font.weight: Font.DemiBold; color: Theme.onSurfaceVariant }
                            CText {
                                visible: card.modelData.permissions.length === 0
                                text: qsTr("Особых разрешений не требует")
                                size: 13
                            }
                            Repeater {
                                model: card.modelData.permissions
                                RowLayout {
                                    required property string modelData
                                    Layout.fillWidth: true
                                    spacing: 8
                                    CookieShape { width: 10; height: 10; color: Theme.primary }
                                    CText { text: modelData; size: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                                }
                            }
                            CText {
                                visible: card.modelData.hostAccess.length > 0
                                text: qsTr("Доступ к сайтам: %1").arg(
                                    card.modelData.hostAccess === "ON_ALL_SITES" ? qsTr("ко всем сайтам")
                                    : card.modelData.hostAccess === "ON_SPECIFIC_SITES" ? qsTr("к выбранным сайтам")
                                    : card.modelData.hostAccess === "ON_CLICK" ? qsTr("по нажатию") : card.modelData.hostAccess)
                                size: 13
                            }
                            CText { text: "ID: " + card.modelData.id; size: 12; color: Theme.onSurfaceVariant; font.family: "Consolas" }
                            CText {
                                visible: card.modelData.path.length > 0
                                text: qsTr("Папка: %1").arg(card.modelData.path)
                                size: 12
                                color: Theme.onSurfaceVariant
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }
                            CText {
                                visible: card.modelData.manifestVersion === 2
                                text: qsTr("Manifest V2 — Chromium 156 такие расширения больше не запускает")
                                size: 12.5
                                color: Theme.error
                            }
                        }
                    }
                }
            }

            // Что поддерживается
            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: 16
                implicitHeight: noteCol.implicitHeight + 28
                radius: 20
                color: Theme.surfaceContainer
                ColumnLayout {
                    id: noteCol
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 4
                    CText { text: qsTr("Как работают расширения в Crumb"); size: 14; font.weight: Font.DemiBold }
                    CText {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        size: 12.5
                        color: Theme.onSurfaceVariant
                        text: qsTr("Crumb использует систему расширений Chromium: блокировщики рекламы, менеджеры паролей, " +
                                   "скрипты на страницах и фоновые сервисы работают как в Chrome. Всплывающее окно расширения " +
                                   "открывается по нажатию на его печеньку на панели. Ограничения: расширения без всплывающего " +
                                   "окна (только chrome.action.onClicked) по нажатию не срабатывают; боковая панель (sidePanel) " +
                                   "не поддерживается; для chrome.tabs каждое окно Crumb — отдельное «окно» Chrome.")
                    }
                }
            }
        }
    }

    Popup {
        id: removePopup
        property var ext: ({})
        anchors.centerIn: parent
        width: 440
        modal: true
        padding: 24
        background: Rectangle { color: Theme.surfaceContainerHigh; radius: 28 }
        contentItem: ColumnLayout {
            spacing: 14
            CText { text: qsTr("Удалить «%1»?").arg(removePopup.ext.name || ""); size: 21; font.weight: Font.DemiBold; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            CText { text: qsTr("Расширение и его данные будут удалены."); color: Theme.onSurfaceVariant }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 18
                CookieButton { variant: "tonal"; icon: ""; text: qsTr("Отмена"); onClicked: removePopup.close() }
                CookieButton {
                    icon: ""; text: qsTr("Удалить")
                    variant: "filled"; color: Theme.error; contentColor: Theme.onError
                    onClicked: { Extensions.remove(removePopup.ext.id); page.setPinned(removePopup.ext.id, false); removePopup.close() }
                }
            }
        }
    }
}
