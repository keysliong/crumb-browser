// Быстрая настройка темы: светлая/тёмная/системная и источник цвета.
// Всё остальное — в разделе «Цвета» страницы настроек.

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Crumb.Ui

CookiePopup {
    id: popup
    padding: 18
    width: 360

    signal openSettings()

    background: Rectangle {
        color: Theme.surfaceContainer
        radius: 22
        border.color: Theme.outlineVariant
    }

    component SectionTitle: CText {
        color: Theme.onSurfaceVariant
        size: 13
        font.weight: Font.DemiBold
        Layout.topMargin: 4
    }

    // Печенька-образец цвета; отмеченная — с галочкой и вдавленными волнами.
    component Swatch: CookieButton {
        property color swatch
        property bool selected: false
        size: 36
        depth: selected ? Prefs.cookieDepth * 2 : Prefs.cookieDepth
        color: swatch
        contentColor: "white"
        icon: selected ? "" : ""
    }

    contentItem: ColumnLayout {
        spacing: 12

        CText { text: qsTr("Тема"); size: 20; font.weight: Font.DemiBold }

        SectionTitle { text: qsTr("Режим") }
        RowLayout {
            spacing: 22
            Repeater {
                model: [
                    { mode: "system", icon: "", label: qsTr("Как в системе") },
                    { mode: "light",  icon: "", label: qsTr("Светлая") },
                    { mode: "dark",   icon: "", label: qsTr("Тёмная") }
                ]
                CookieButton {
                    required property var modelData
                    icon: modelData.icon
                    text: modelData.label
                    labelPosition: "below"
                    size: 48
                    checked: Theme.themeMode === modelData.mode
                    onClicked: Theme.themeMode = modelData.mode
                }
            }
        }

        SectionTitle { text: qsTr("Цвета из обоев") }
        Flow {
            Layout.fillWidth: true
            spacing: 10
            Repeater {
                model: Theme.wallpaperColors
                Swatch {
                    required property color modelData
                    required property int index
                    swatch: modelData
                    selected: Theme.sourceType === "wallpaper" && index === 0
                    toolTip: index === 0 ? qsTr("Основной цвет обоев") : qsTr("Цвет из обоев")
                    onClicked: index === 0 ? Theme.sourceType = "wallpaper" : Theme.customColor = modelData
                }
            }
        }

        SectionTitle { text: qsTr("Готовые цвета") }
        Flow {
            Layout.fillWidth: true
            spacing: 10
            Repeater {
                model: Theme.presetColors
                Swatch {
                    required property color modelData
                    swatch: modelData
                    selected: Theme.sourceType === "custom" && Qt.colorEqual(Theme.customColor, modelData)
                    onClicked: Theme.customColor = modelData
                }
            }
        }

        CookieSwitch {
            Layout.fillWidth: true
            Layout.topMargin: 4
            text: qsTr("Чёрная тема для AMOLED")
            checked: Theme.amoled
            enabled: Theme.dark
            onToggled: (on) => Theme.amoled = on
        }

        CookieButton {
            Layout.topMargin: 4
            icon: ""
            text: qsTr("Все настройки")
            variant: "surface"
            onClicked: popup.openSettings()
        }
    }
}
