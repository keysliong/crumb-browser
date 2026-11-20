// «Сохранить пароль?» — появляется под адресной строкой после входа на сайт.
// Логин можно поправить, пароль — посмотреть. «Никогда» — для этого сайта больше не спрашивать.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Crumb
import Crumb.Ui

CookiePopup {
    id: sp
    width: 380
    implicitWidth: 380
    padding: 18
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    property string url: ""
    property string mode: "new"          // new | update
    property string password: ""
    property bool reveal: false
    signal openManager()

    function offer(u, user, pass, m) {
        url = u; mode = m; password = pass; reveal = false
        userField.text = user
        open()
        autoClose.restart()
    }

    background: Rectangle { color: Theme.surfaceContainer; radius: 28; border.color: Theme.outlineVariant }
    // Не мешаем вечно: через 25 с без действий окно уходит само.
    Timer { id: autoClose; interval: 25000; onTriggered: sp.close() }

    component Field: Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 44
        radius: 22
        color: Theme.surfaceContainerHighest
    }

    contentItem: ColumnLayout {
        spacing: 12
        RowLayout {
            spacing: 12
            CookieShape {
                width: 44; height: 44
                color: Theme.primaryContainer
                spin: Prefs.animations ? 10 : 0
                Text {
                    anchors.centerIn: parent
                    rotation: -parent.rotation
                    text: ""; font.family: Theme.iconFont; font.pixelSize: 18
                    color: Theme.onPrimaryContainer
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                CText {
                    text: sp.mode === "update" ? qsTr("Обновить пароль?") : qsTr("Сохранить пароль?")
                    size: 17; font.weight: Font.DemiBold
                }
                CText {
                    text: Passwords.siteOf(sp.url)
                    size: 13; color: Theme.onSurfaceVariant
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }
            }
        }
        Field {
            border.width: userField.activeFocus ? 2 : 0
            border.color: Theme.primary
            Text {
                id: userGlyph
                anchors.left: parent.left; anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: ""; font.family: Theme.iconFont; font.pixelSize: 13
                color: Theme.onSurfaceVariant
            }
            TextInput {
                id: userField
                anchors.left: userGlyph.right; anchors.leftMargin: 12
                anchors.right: parent.right; anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                font.family: Prefs.font; font.pixelSize: Prefs.px(14)
                color: Theme.onSurface
                selectByMouse: true
                selectionColor: Theme.primaryContainer
                clip: true
                onTextEdited: autoClose.restart()
                CText {
                    anchors.fill: parent; verticalAlignment: Text.AlignVCenter
                    visible: userField.text.length === 0
                    text: qsTr("Имя пользователя (необязательно)")
                    color: Theme.onSurfaceVariant
                }
            }
        }
        Field {
            Text {
                id: keyGlyph
                anchors.left: parent.left; anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: ""; font.family: Theme.iconFont; font.pixelSize: 13
                color: Theme.onSurfaceVariant
            }
            CText {
                anchors.left: keyGlyph.right; anchors.leftMargin: 12
                anchors.right: eye.left; anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                text: sp.reveal ? sp.password : "•".repeat(Math.min(sp.password.length, 24))
                size: 14
                elide: Text.ElideRight
            }
            CookieButton {
                id: eye
                anchors.right: parent.right; anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                size: 32; variant: "surface"
                icon: sp.reveal ? "" : ""
                toolTip: sp.reveal ? qsTr("Скрыть") : qsTr("Показать пароль")
                onClicked: { sp.reveal = !sp.reveal; autoClose.restart() }
            }
        }
        RowLayout {
            Layout.topMargin: 4
            spacing: 6
            CookieButton {
                size: 34; variant: "surface"; icon: ""
                text: sp.mode === "update" ? qsTr("Нет") : qsTr("Никогда")
                toolTip: sp.mode === "update" ? "" : qsTr("Не предлагать для этого сайта")
                onClicked: { if (sp.mode !== "update") Passwords.neverFor(sp.url); sp.close() }
            }
            Item { Layout.fillWidth: true }
            CookieButton {
                size: 34; variant: "surface"; icon: ""
                visible: sp.mode !== "update"
                toolTip: qsTr("Не сейчас")
                onClicked: sp.close()
            }
            CookieButton {
                size: 34; variant: "filled"; icon: ""
                text: sp.mode === "update" ? qsTr("Обновить") : qsTr("Сохранить")
                onClicked: { Passwords.save(sp.url, userField.text.trim(), sp.password); sp.close(); sp.saved() }
            }
        }
    }
    signal saved()
}
