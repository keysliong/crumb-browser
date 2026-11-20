// Окно установщика/удалятора Crumb — по образцу установщика Notepad You:
// те же шаги, раскладка и анимации, но все кнопки — печеньки (текст рядом).

import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import QtQuick.Dialogs
import Crumb.Ui
import CrumbSetup

Window {
    id: win

    width: 780
    height: 500
    minimumWidth: 780; maximumWidth: 780
    minimumHeight: 500; maximumHeight: 500
    visible: true
    flags: Qt.Window | Qt.FramelessWindowHint
    color: "transparent"
    title: Setup.uninstallMode ? qsTr("Удаление Crumb") : qsTr("Установка Crumb")

    // ------------------------------------------------------- состояние
    property string page: Setup.uninstallMode ? (Setup.alreadyInstalled ? "uninstallConfirm" : "notInstalled")
                                              : "welcome"
    property real pageDir: 1
    property real time: 0
    property real shown: 0                // сглаженный прогресс
    property real doneT: 0

    function go(p, dir) {
        pageDir = dir
        page = p
        enterAnim.restart()
    }

    function startWork() {
        shown = 0
        Setup.start()
        go("progress", 1)
    }

    // Обычный таймер, а не FrameAnimation: Qt не рисует перекрытое окно, и логика
    // (переход на «Готово») не должна останавливаться вместе с отрисовкой.
    property real lastTick: Date.now()
    Timer {
        interval: 16
        repeat: true
        running: true
        onTriggered: {
            const now = Date.now()
            const frameTime = Math.min((now - win.lastTick) / 1000, 0.1)
            win.lastTick = now
            win.time += frameTime
            if (win.page === "progress") {
                const dt = Math.min(frameTime, 0.05)
                win.shown += (Setup.progress - win.shown) * (1 - Math.exp(-dt * 6))
                // Даём анимации прогресса дойти до конца
                if (Setup.finished && (Setup.error.length > 0 || win.shown > 0.985)) {
                    if (Setup.error.length > 0) win.go("error", 1)
                    else win.go(Setup.uninstallMode ? "uninstallDone" : "done", 1)
                }
            }
        }
    }

    readonly property bool isDone: page === "done" || page === "uninstallDone"
    onIsDoneChanged: if (isDone) { doneT = 0; doneAnim.restart() }
    NumberAnimation { id: doneAnim; target: win; property: "doneT"; from: 0; to: 1; duration: 600; easing.type: Easing.OutCubic }

    // ------------------------------------------------------------- фон
    Rectangle {
        anchors.fill: parent
        radius: 18
        color: Theme.surface
        border.color: Theme.outlineVariant
        border.width: 1
    }

    Hero {
        id: hero
        x: 10; y: 10
        width: 290
        height: win.height - 20
        time: win.time
        uninstall: Setup.uninstallMode
        version: Setup.version
        progress: win.page === "progress" ? win.shown : (win.isDone ? 1 : -1)
        doneT: win.isDone ? win.doneT : 0
    }

    // Перетаскивание окна за верхнюю полосу
    Item {
        x: 0; y: 0
        width: win.width - 110
        height: 46
        DragHandler { target: null; onActiveChanged: if (active) win.startSystemMove() }
    }

    // Кнопки окна
    Row {
        anchors.right: parent.right
        anchors.rightMargin: 16
        y: 14
        spacing: 8
        CookieButton {
            size: 32; variant: "surface"; icon: ""
            toolTip: qsTr("Свернуть")
            onClicked: win.showMinimized()
        }
        CookieButton {
            size: 32; variant: "surface"; icon: ""
            toolTip: qsTr("Закрыть")
            enabled: win.page !== "progress"
            onClicked: Setup.quit()
        }
    }

    // ---------------------------------------------------------- страницы
    Item {
        id: content
        x: hero.x + hero.width + 34 + (1 - enter) * 56 * win.pageDir
        y: 60
        width: win.width - hero.width - 10 - 34 - 34
        height: win.height - 60 - 28
        opacity: enter
        property real enter: 1
        NumberAnimation on enter { id: enterAnim; from: 0; to: 1; duration: 420; easing.type: Easing.OutCubic; running: false }

        Loader {
            anchors.fill: parent
            sourceComponent: ({
                welcome: welcomePage, options: optionsPage, progress: progressPage,
                done: donePage, error: errorPage, uninstallConfirm: uninstallPage,
                uninstallDone: uninstallDonePage, notInstalled: notInstalledPage
            })[win.page]
        }
    }

    // ------------------------------------------------ общие кусочки страниц
    component Title: Text {
        color: Theme.onSurface
        font.pixelSize: 32
        font.weight: Font.Bold
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
    }
    component Body: Text {
        color: Theme.onSurfaceVariant
        font.pixelSize: 15
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        lineHeight: 1.15
    }
    component Chip: Rectangle {
        property alias text: chipLabel.text
        implicitWidth: chipLabel.implicitWidth + 24
        implicitHeight: 26
        radius: 10
        color: Theme.secondaryContainer
        Text {
            id: chipLabel
            anchors.centerIn: parent
            color: Theme.onSecondaryContainer
            font.pixelSize: 12
        }
    }
    // Пункт со значком в печеньке; появляется с задержкой по порядку.
    component FeatureRow: Item {
        id: fr
        property string icon
        property string text
        property int order: 0
        property bool animate: true
        Layout.fillWidth: true
        implicitHeight: 40
        property real t: 0
        Component.onCompleted: animate ? appear.start() : (t = 1)
        SequentialAnimation {
            id: appear
            PauseAnimation { duration: 150 + fr.order * 90 }
            NumberAnimation { target: fr; property: "t"; to: 1; duration: 400; easing.type: Easing.OutCubic }
        }
        opacity: t
        CookieShape {
            x: 1 + (1 - fr.t) * 24
            anchors.verticalCenter: parent.verticalCenter
            width: 34; height: 34
            waves: 8; depth: 0.12
            rotation: fr.t * 46
            color: Theme.secondaryContainer
            Text {
                anchors.centerIn: parent
                rotation: -parent.rotation
                text: fr.icon
                font.family: Theme.iconFont
                font.pixelSize: 14
                color: Theme.onSecondaryContainer
            }
        }
        Text {
            x: 48 + (1 - fr.t) * 24
            width: parent.width - 48
            anchors.verticalCenter: parent.verticalCenter
            text: fr.text
            elide: Text.ElideMiddle
            color: Theme.onSurface
            font.pixelSize: 15
        }
    }
    component SwitchRow: CookieSwitch {
        Layout.fillWidth: true
        Layout.preferredHeight: 42
    }
    // Нижняя полоса кнопок: первая в списке — справа.
    component BottomBar: Row {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        layoutDirection: Qt.RightToLeft
        spacing: 18
    }

    // ============================================================ welcome
    Component {
        id: welcomePage
        Item {
            ColumnLayout {
                width: parent.width
                spacing: 0
                Chip {
                    text: Setup.alreadyInstalled ? qsTr("Уже установлен · обновление до %1").arg(Setup.version)
                                                 : qsTr("Установщик · версия %1").arg(Setup.version)
                }
                Title { text: qsTr("Добро пожаловать!"); Layout.topMargin: 10 }
                Body { text: qsTr("Сейчас мы установим Crumb — браузер на движке Chromium в стиле Material You.") }
                Item { implicitHeight: 14 }
                FeatureRow { order: 0; icon: "\uE8D6"; text: qsTr("Яндекс Музыка и видео — кодеки AAC/H.264") }
                FeatureRow { order: 1; icon: "\uE81C"; text: qsTr("Rewind — машина времени для каждой страницы") }
                FeatureRow { order: 2; icon: "\uE77B"; text: qsTr("Аккаунт: настройки и расширения на всех ПК") }
                FeatureRow { order: 3; icon: "\uE790"; text: qsTr("Цвета из обоев, печеньки и живые анимации") }
            }
            BottomBar {
                CookieButton {
                    variant: "filled"; icon: ""; size: 44
                    text: Setup.alreadyInstalled ? qsTr("Обновить") : qsTr("Установить")
                    enabled: Setup.hasPayload
                    onClicked: win.startWork()
                }
                CookieButton {
                    variant: "tonal"; icon: ""; size: 44
                    text: qsTr("Параметры")
                    onClicked: win.go("options", 1)
                }
            }
        }
    }

    // ============================================================ options
    Component {
        id: optionsPage
        Item {
            FolderDialog {
                id: folderDialog
                title: qsTr("Куда установить Crumb?")
                onAccepted: Setup.chooseFolder(selectedFolder)
            }
            ColumnLayout {
                width: parent.width
                spacing: 0
                Title { text: qsTr("Параметры установки"); font.pixelSize: 28 }
                Text {
                    Layout.topMargin: 10
                    text: qsTr("Папка установки")
                    color: Theme.onSurfaceVariant
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 6
                    spacing: 12
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42
                        radius: 21
                        color: Theme.surfaceContainerHigh
                        border.width: dirInput.activeFocus ? 2 : 0
                        border.color: Theme.primary
                        TextInput {
                            id: dirInput
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14
                            verticalAlignment: TextInput.AlignVCenter
                            clip: true
                            selectByMouse: true
                            text: Setup.installDir.replace(/\//g, "\\")
                            color: Theme.onSurface
                            selectionColor: Theme.primaryContainer
                            selectedTextColor: Theme.onPrimaryContainer
                            font.pixelSize: 14
                            onTextEdited: Setup.installDir = text
                        }
                    }
                    CookieButton {
                        variant: "tonal"; icon: ""; size: 40
                        text: qsTr("Обзор…")
                        onClicked: folderDialog.open()
                    }
                }
                Item { implicitHeight: 10 }
                SwitchRow {
                    text: qsTr("Ярлык на рабочем столе")
                    checked: Setup.desktopShortcut
                    onToggled: (on) => Setup.desktopShortcut = on
                }
                SwitchRow {
                    text: qsTr("Ярлык в меню «Пуск»")
                    checked: Setup.startMenuShortcut
                    onToggled: (on) => Setup.startMenuShortcut = on
                }
                SwitchRow {
                    text: qsTr("Открывать ссылки и .html через Crumb")
                    checked: Setup.registerBrowser
                    onToggled: (on) => Setup.registerBrowser = on
                }
                Text {
                    Layout.topMargin: 6
                    text: qsTr("Потребуется около %1 МБ. Права администратора не нужны.").arg(Math.round(Setup.requiredMB))
                    color: Theme.onSurfaceVariant
                    font.pixelSize: 13
                }
            }
            BottomBar {
                CookieButton {
                    variant: "filled"; icon: ""; size: 44
                    text: Setup.alreadyInstalled ? qsTr("Обновить") : qsTr("Установить")
                    enabled: Setup.hasPayload && Setup.installDir.length > 0
                    onClicked: win.startWork()
                }
                CookieButton {
                    variant: "outlined"; icon: ""; size: 44
                    text: qsTr("Назад")
                    onClicked: win.go("welcome", -1)
                }
            }
        }
    }

    // =========================================================== progress
    Component {
        id: progressPage
        ColumnLayout {
            spacing: 0
            Title {
                font.pixelSize: 30
                text: (Setup.uninstallMode ? qsTr("Удаляем") : qsTr("Устанавливаем"))
                      + ".".repeat(1 + Math.floor(win.time * 2.5) % 3)
            }
            Body { text: Setup.status; font.pixelSize: 15 }
            Text {
                Layout.topMargin: 18
                text: Math.round(win.shown * 100) + "%"
                color: Theme.primary
                font.pixelSize: 44
                font.weight: Font.Bold
            }
            // Волнистая полоса прогресса
            Shape {
                id: bar
                Layout.fillWidth: true
                Layout.preferredHeight: 22
                preferredRendererType: Shape.CurveRenderer
                readonly property real x0: 3
                readonly property real x1: width - 3
                readonly property real xe: x0 + (x1 - x0) * Math.max(0, Math.min(1, win.shown))
                readonly property real yc: height / 2
                ShapePath {
                    strokeColor: Qt.alpha(Theme.primary, 0.18)
                    strokeWidth: 5
                    capStyle: ShapePath.RoundCap
                    fillColor: "transparent"
                    PathPolyline { path: bar.xe + 8 < bar.x1 ? [Qt.point(bar.xe + 8, bar.yc), Qt.point(bar.x1, bar.yc)] : [] }
                }
                ShapePath {
                    strokeColor: Theme.primary
                    strokeWidth: 5
                    capStyle: ShapePath.RoundCap
                    fillColor: "transparent"
                    PathPolyline {
                        path: {
                            const pts = []
                            if (bar.xe <= bar.x0 + 1) return pts
                            const n = Math.floor((bar.xe - bar.x0) / 2) + 2
                            for (let i = 0; i <= n; ++i) {
                                const x = bar.x0 + (bar.xe - bar.x0) * i / n
                                pts.push(Qt.point(x, bar.yc + 4 * Math.sin((x - bar.x0) / 9 - win.time * 6)))
                            }
                            return pts
                        }
                    }
                }
                Rectangle {
                    visible: bar.xe + 8 < bar.x1
                    width: 5; height: 5; radius: 2.5
                    x: bar.x1 - 2.5; y: bar.yc - 2.5
                    color: Theme.primary
                }
            }
            Item { implicitHeight: 16 }
            Repeater {
                model: Setup.uninstallMode
                       ? [[qsTr("Удаление ярлыков"), 0.35], [qsTr("Очистка реестра"), 0.6], [qsTr("Удаление файлов"), 1.0]]
                       : [[qsTr("Копирование файлов"), 0.6], [qsTr("Создание ярлыков"), 0.82], [qsTr("Регистрация в системе"), 1.0]]
                Item {
                    required property var modelData
                    required property int index
                    readonly property real prev: index === 0 ? 0
                        : (Setup.uninstallMode ? [0, 0.35, 0.6] : [0, 0.6, 0.82])[index]
                    readonly property int st: win.shown >= modelData[1] - 0.005 ? 2 : (win.shown >= prev ? 1 : 0)
                    Layout.fillWidth: true
                    implicitHeight: 36
                    // готово — печенька с галочкой, идёт — крутящаяся печенька, ждёт — кружок
                    CookieShape {
                        visible: parent.st > 0
                        x: parent.st === 2 ? 3 : 4
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.st === 2 ? 26 : 24; height: width
                        waves: parent.st === 2 ? 8 : 7
                        depth: parent.st === 2 ? 0.12 : 0.16
                        rotation: parent.st === 1 ? win.time * 143 : 0
                        color: parent.st === 2 ? Theme.primary : Qt.alpha(Theme.primary, 0.75)
                        Text {
                            anchors.centerIn: parent
                            visible: parent.parent.st === 2
                            text: ""
                            font.family: Theme.iconFont
                            font.pixelSize: 11
                            color: Theme.onPrimary
                        }
                    }
                    Rectangle {
                        visible: parent.st === 0
                        x: 6
                        anchors.verticalCenter: parent.verticalCenter
                        width: 20; height: 20; radius: 10
                        color: "transparent"
                        border.width: 2
                        border.color: Theme.outlineVariant
                    }
                    Text {
                        x: 42
                        anchors.verticalCenter: parent.verticalCenter
                        text: parent.modelData[0]
                        color: parent.st === 0 ? Theme.onSurfaceVariant : Theme.onSurface
                        font.pixelSize: 15
                    }
                }
            }
        }
    }

    // =============================================================== done
    Component {
        id: donePage
        Item {
            ColumnLayout {
                width: parent.width
                spacing: 0
                Title {
                    id: doneTitle
                    text: qsTr("Готово!")
                    property real pop: 0
                    font.pixelSize: 26 + 10 * pop
                    NumberAnimation on pop { from: 0; to: 1; duration: 500; easing.type: Easing.OutBack }
                }
                Body { text: qsTr("Crumb установлен и готов к работе.") }
                Item { implicitHeight: 14 }
                FeatureRow { animate: false; icon: ""; text: qsTr("Папка: %1").arg(Setup.installDir.replace(/\//g, "\\")) }
                FeatureRow { animate: false; visible: Setup.startMenuShortcut; icon: ""; text: qsTr("Ярлык в меню «Пуск»") }
                FeatureRow { animate: false; visible: Setup.desktopShortcut; icon: "\uE7F4"; text: qsTr("Ярлык на рабочем столе") }
                FeatureRow { animate: false; visible: Setup.registerBrowser; icon: "\uE774"; text: qsTr("Crumb доступен как браузер по умолчанию") }
                Item { implicitHeight: 8 }
                SwitchRow {
                    text: qsTr("Запустить Crumb")
                    checked: Setup.launchAfter
                    onToggled: (on) => Setup.launchAfter = on
                }
            }
            BottomBar {
                CookieButton {
                    variant: "filled"; icon: ""; size: 44
                    text: qsTr("Готово")
                    onClicked: Setup.quit()
                }
            }
        }
    }

    // ============================================================== error
    Component {
        id: errorPage
        Item {
            ColumnLayout {
                width: parent.width
                spacing: 0
                Title { text: qsTr("Что-то пошло не так"); font.pixelSize: 30 }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.topMargin: 12
                    implicitHeight: errText.implicitHeight + 32
                    radius: 16
                    color: Theme.secondaryContainer
                    Text {
                        id: errText
                        anchors.fill: parent
                        anchors.margins: 16
                        text: Setup.error
                        wrapMode: Text.WordWrap
                        color: Theme.onSecondaryContainer
                        font.pixelSize: 14
                    }
                }
            }
            BottomBar {
                CookieButton {
                    variant: "filled"; icon: ""; size: 44
                    text: qsTr("Закрыть")
                    onClicked: Setup.quit()
                }
                CookieButton {
                    variant: "outlined"; icon: ""; size: 44
                    text: qsTr("Попробовать снова")
                    onClicked: win.go(Setup.uninstallMode ? "uninstallConfirm" : "welcome", -1)
                }
            }
        }
    }

    // ================================================== uninstall confirm
    Component {
        id: uninstallPage
        Item {
            ColumnLayout {
                width: parent.width
                spacing: 0
                Title { text: qsTr("Удалить Crumb?"); font.pixelSize: 30 }
                Body { text: qsTr("История, закладки и настройки останутся, если не отметить пункт ниже."); font.pixelSize: 15 }
                Item { implicitHeight: 6 }
                FeatureRow { order: 0; icon: ""; text: qsTr("Программа: %1").arg(Setup.shortPath(Setup.installDir, 34)) }
                FeatureRow { order: 1; icon: ""; text: qsTr("Ярлыки в «Пуске» и на рабочем столе") }
                FeatureRow { order: 2; icon: "\uE713"; text: qsTr("Запись в «Установленных приложениях» и регистрация браузера") }
                SwitchRow {
                    text: qsTr("Удалить также историю, закладки и настройки")
                    checked: Setup.removeUserData
                    onToggled: (on) => Setup.removeUserData = on
                    opacity: 0
                    NumberAnimation on opacity { to: 1; duration: 400; easing.type: Easing.OutCubic; running: true }
                }
            }
            BottomBar {
                CookieButton {
                    variant: "filled"; icon: ""; size: 44
                    text: qsTr("Удалить")
                    onClicked: win.startWork()
                }
                CookieButton {
                    variant: "tonal"; icon: ""; size: 44
                    text: qsTr("Отмена")
                    onClicked: Setup.quit()
                }
            }
        }
    }

    // ===================================================== uninstall done
    Component {
        id: uninstallDonePage
        Item {
            ColumnLayout {
                width: parent.width
                spacing: 0
                Title {
                    text: qsTr("Crumb удалён")
                    property real pop: 0
                    font.pixelSize: 22 + 8 * pop
                    NumberAnimation on pop { from: 0; to: 1; duration: 500; easing.type: Easing.OutBack }
                }
                Body { text: qsTr("Спасибо, что пользовались! Будем рады видеть вас снова.") }
                Item { implicitHeight: 14 }
                FeatureRow { order: 1; icon: ""; text: qsTr("Программа удалена") }
                FeatureRow { order: 2; icon: ""; text: qsTr("Ярлыки убраны") }
                FeatureRow { order: 3; icon: ""; text: qsTr("Система очищена") }
            }
            BottomBar {
                CookieButton {
                    variant: "filled"; icon: ""; size: 44
                    text: qsTr("Закрыть")
                    onClicked: Setup.quit()
                }
            }
        }
    }

    // ====================================================== not installed
    Component {
        id: notInstalledPage
        Item {
            ColumnLayout {
                width: parent.width
                spacing: 0
                Chip { text: qsTr("Удаление · версия %1").arg(Setup.version) }
                Title { text: qsTr("Crumb не найден"); font.pixelSize: 30; Layout.topMargin: 10 }
                Body { text: qsTr("Похоже, программа уже удалена или установлена в другую папку. Удалять нечего.") }
            }
            BottomBar {
                CookieButton {
                    variant: "filled"; icon: ""; size: 44
                    text: qsTr("Закрыть")
                    onClicked: Setup.quit()
                }
            }
        }
    }
}
