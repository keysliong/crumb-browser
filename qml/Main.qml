// Главное окно Crumb: вкладки (сверху или сбоку), панель, страницы.
// Окно без системной рамки — кнопки «свернуть/развернуть/закрыть» тоже печеньки.
// У вкладки есть вид: home — домашняя страница Crumb, settings — настройки,
// web — сайт (BrowserView создаётся при первом переходе).

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Crumb
import Crumb.Ui

Window {
    id: win

    width: 1280
    height: 820
    minimumWidth: 720
    minimumHeight: 460
    visible: true
    flags: Qt.Window | Qt.FramelessWindowHint
    color: Theme.surfaceContainer
    title: currentPage && currentPage.title.length ? currentPage.title + " — Crumb" : "Crumb"

    readonly property bool maximized: visibility === Window.Maximized
    readonly property int edge: maximized ? 0 : 6
    readonly property bool sideTabs: Settings.tabsPosition === "side"
    readonly property bool compact: Settings.toolbarCompact

    // ------------------------------------------------------------ модель
    // tabs: uid, kind (home|settings|downloads|extensions|history|passwords|web), url, webStarted, fromHome, groupId,
    //       lazy (восстановлена, сайт загрузится при первом открытии), savedTitle
    ListModel { id: tabs }
    // groups: gid, name, colorIndex, collapsed
    ListModel { id: groups }

    property int currentIndex: -1
    property int nextUid: 1
    property int nextGid: 1
    property var closedTabs: []          // для «вернуть закрытую вкладку»
    readonly property var currentPage: { pages.count; return pageAt(currentIndex) }

    onCurrentIndexChanged: {
        wake(currentIndex)
        findBar.close()
        hidePanels()
        scheduleSave()
        toolbarAddress.cancelEditing()
        if (currentPage && currentPage.kind === "home") Qt.callLater(() => toolbarAddress.focusAddress())
    }

    function pageAt(i) { return i >= 0 && i < pages.count ? pages.itemAt(i) : null }

    function newTab(kind, url, index, activate, groupId) {
        const at = (index === undefined || index < 0 || index > tabs.count) ? tabs.count : index
        tabs.insert(at, { uid: nextUid++, kind: kind, url: url || "", webStarted: kind === "web",
                          fromHome: false, groupId: groupId === undefined ? -1 : groupId, lazy: false, savedTitle: "" })
        if (activate === undefined || activate) {
            currentIndex = at
            if (kind === "home") Qt.callLater(() => toolbarAddress.focusAddress())
        } else if (at <= currentIndex) {
            currentIndex++
        }
    }
    function newHomeTab() { newTab("home") }

    // Внутренние страницы Crumb: одна вкладка на страницу.
    function openPage(kind) {
        for (let i = 0; i < tabs.count; ++i)
            if (tabs.get(i).kind === kind) { activate(i); return }
        newTab(kind, "", currentIndex + 1)
    }
    function openSettings() { openPage("settings") }
    // Открыть адрес новой вкладкой: crumb://… — внутренняя страница, остальное — сайт.
    function openUrl(u, index) {
        const t = String(u).trim()
        if (internalKind(t)) openPage(internalKind(t))
        else if (t === "crumb://home" || t === "crumb://newtab" || t.startsWith("chrome://newtab")
                 || t.startsWith("chrome://new-tab-page")) newTab("home", "", index)
        else newTab("web", t, index)
    }
    readonly property var internalPages: ({ "crumb://settings": "settings", "crumb://downloads": "downloads",
                                            "crumb://extensions": "extensions", "crumb://history": "history",
                                            "crumb://passwords": "passwords" })
    // Своя страница Crumb для адреса (в т.ч. chrome://extensions и chrome://downloads) или "".
    function internalKind(u) {
        const t = String(u).trim().toLowerCase().replace(/\/+$/, "")
        if (internalPages[t]) return internalPages[t]
        if (/^chrome:\/\/extensions([\/?#]|$)/.test(t)) return "extensions"
        if (/^chrome:\/\/downloads([\/?#]|$)/.test(t)) return "downloads"
        if (/^chrome:\/\/history([\/?#]|$)/.test(t)) return "history"
        if (/^chrome:\/\/(password-manager|settings\/passwords)([\/?#]|$)/.test(t)) return "passwords"
        return ""
    }

    // Нажатие на печеньку расширения: его всплывающее окно под кнопкой.
    function openExtension(ext, anchorItem) {
        if (ext.hasPopup) extPopup.openAt(anchorItem, ext.popupUrl)
        else if (ext.optionsUrl.length) newTab("web", ext.optionsUrl, currentIndex + 1)
        else openPage("extensions")
    }

    function closeTab(i) {
        if (i < 0 || i >= tabs.count) return
        const p = pageAt(i)
        if (p) closedTabs = closedTabs.concat([{ kind: tabs.get(i).kind, url: p.currentUrl || tabs.get(i).url, index: i,
                                                 groupId: tabs.get(i).groupId, title: p.title }]).slice(-25)
        if (tabs.count === 1) { close(); return }   // последняя вкладка — закрываем окно
        const wasCurrent = i === currentIndex
        tabs.remove(i)
        if (i < currentIndex || (wasCurrent && currentIndex >= tabs.count)) currentIndex--
        cleanupGroups()
    }
    function closeOthers(i) { for (let k = tabs.count - 1; k >= 0; --k) if (k !== i) closeTab(k) }

    function reopenClosed() { reopenClosedAt(closedTabs.length - 1) }
    function reopenClosedAt(k) {
        if (k < 0 || k >= closedTabs.length) return
        const t = closedTabs[k]
        const rest = closedTabs.slice()
        rest.splice(k, 1)
        closedTabs = rest
        const kind = t.kind === "web" && t.url.length ? "web" : t.kind
        newTab(kind, t.url, Math.min(t.index, tabs.count), true, groupExists(t.groupId) ? t.groupId : -1)
    }

    function activate(i) {
        if (i < 0 || i >= tabs.count) return
        currentIndex = i
        const g = groupIndex(tabs.get(i).groupId)
        if (g >= 0 && groups.get(g).collapsed) groups.setProperty(g, "collapsed", false)
    }
    function selectRelative(step) { if (tabs.count > 1) activate((currentIndex + step + tabs.count) % tabs.count) }
    function duplicate(i) {
        const t = tabs.get(i), p = pageAt(i)
        newTab(t.kind, p ? p.currentUrl : t.url, i + 1, true, t.groupId)
    }

    // --------------------------------------------------------- группы
    function groupIndex(gid) {
        for (let k = 0; k < groups.count; ++k) if (groups.get(k).gid === gid) return k
        return -1
    }
    function groupExists(gid) { return gid >= 0 && groupIndex(gid) >= 0 }
    function addToNewGroup(i) {
        const gid = nextGid++
        groups.append({ gid: gid, name: qsTr("Группа %1").arg(gid), colorIndex: (gid - 1) % Theme.groupColorCount(), collapsed: false })
        tabs.setProperty(i, "groupId", gid)
        renamePopup.openFor(gid)
    }
    function removeFromGroup(i) { tabs.setProperty(i, "groupId", -1); cleanupGroups() }
    function ungroup(gid) {
        for (let k = 0; k < tabs.count; ++k) if (tabs.get(k).groupId === gid) tabs.setProperty(k, "groupId", -1)
        cleanupGroups()
    }
    function closeGroup(gid) { for (let k = tabs.count - 1; k >= 0; --k) if (tabs.get(k).groupId === gid) closeTab(k) }
    function cleanupGroups() {
        for (let g = groups.count - 1; g >= 0; --g) {
            let used = false
            for (let k = 0; k < tabs.count; ++k) if (tabs.get(k).groupId === groups.get(g).gid) used = true
            if (!used) groups.remove(g)
        }
    }

    // ------------------------------------------- действия (клавиши, меню)
    function action(name, fromIndex) {
        const i = fromIndex === undefined ? currentIndex : fromIndex
        const p = pageAt(i)
        switch (name) {
        case "newTab": newHomeTab(); break
        case "closeTab": closeTab(i); break
        case "reopenTab": reopenClosed(); break
        case "nextTab": selectRelative(1); break
        case "prevTab": selectRelative(-1); break
        case "address": toolbarAddress.focusAddress(); break
        case "reload": if (p) p.reload(); break
        case "back": if (p) p.goBack(); break
        case "forward": if (p) p.goForward(); break
        case "home": if (p) p.goHome(); break
        case "settings": openSettings(); break
        case "downloads": openPage("downloads"); break
        case "extensions": openPage("extensions"); break
        case "rewind": openRewind(); break
        case "history": openPage("history"); break
        case "passwords": openPage("passwords"); break
        case "find": openFind(); break
        }
    }

    // Команды Chrome style (Ctrl+T и т.п., если сочетание не переназначено)
    function command(id, fromIndex) {
        switch (id) {
        case BrowserView.CmdNewTab:
        case BrowserView.CmdNewWindow:
        case BrowserView.CmdNewIncognitoWindow: newHomeTab(); break
        case BrowserView.CmdNewTabToRight: newTab("home", "", fromIndex + 1); break
        case BrowserView.CmdCloseTab: closeTab(fromIndex); break
        case BrowserView.CmdCloseWindow:
        case BrowserView.CmdExit: close(); break
        case BrowserView.CmdSelectNextTab: selectRelative(1); break
        case BrowserView.CmdSelectPreviousTab: selectRelative(-1); break
        case BrowserView.CmdSelectLastTab: activate(tabs.count - 1); break
        case BrowserView.CmdDuplicateTab: duplicate(fromIndex); break
        case BrowserView.CmdRestoreTab: reopenClosed(); break
        case BrowserView.CmdFind: openFind(); break
        case BrowserView.CmdFindNext: findBar.opened ? findBar.next() : openFind(); break
        case BrowserView.CmdFindPrevious: findBar.opened ? findBar.prev() : openFind(); break
        default:
            if (id >= BrowserView.CmdSelectTab0 && id <= BrowserView.CmdSelectTab7)
                activate(id - BrowserView.CmdSelectTab0)
        }
    }

    // Окно закрывается только после того, как CEF закроет все браузеры.
    onClosing: (close) => {
        close.accepted = false
        saveSession()
        sessionFrozen = true
        hidePanels()
        CefRuntime.requestQuit()
    }

    Component.onCompleted: {
        WindowEffects.roundCorners(win, true)
        if (CefRuntime.env("CRUMB_MEDIATEST").length) urlsForTest()
        const urls = CefRuntime.startupUrls()
        const restored = Settings.startupMode === "restore" && !CefRuntime.env("CRUMB_MEDIATEST").length && restoreSession()
        if (urls.length > 0) urls.forEach(u => openUrl(u))
        else if (!restored) newHomeTab()
        sessionReady = true
    }
    // Медиа-самотест: что умеет движок в защищённом контексте (https) —
    // MSE-кодеки, <audio>.canPlayType и Widevine (EME). Результат — в stderr.
    function urlsForTest() { newTab("web", "https://music.yandex.ru/?crumb-mediatest") }
    readonly property string mediaTestJs: "(async()=>{const r={};" +
        "const ms=(t)=>{try{return MediaSource.isTypeSupported(t)}catch(e){return 'err'}};" +
        "r.mseAAC=ms('audio/mp4; codecs=\\\"mp4a.40.2\\\"');r.mseMP3=ms('audio/mpeg');" +
        "r.mseOpus=ms('audio/webm; codecs=\\\"opus\\\"');r.mseFLAC=ms('audio/mp4; codecs=\\\"flac\\\"');" +
        "r.mseH264=ms('video/mp4; codecs=\\\"avc1.42E01E\\\"');const a=document.createElement('audio');" +
        "r.tagAAC=a.canPlayType('audio/aac');r.tagMP3=a.canPlayType('audio/mpeg');r.tagM4A=a.canPlayType('audio/mp4');" +
        "r.secure=window.isSecureContext;r.eme=!!navigator.requestMediaKeySystemAccess;" +
        "for(const [n,c] of [['wvAAC','audio/mp4; codecs=\\\"mp4a.40.2\\\"'],['wvOpus','audio/webm; codecs=\\\"opus\\\"']]){" +
        "try{await navigator.requestMediaKeySystemAccess('com.widevine.alpha',[{initDataTypes:['cenc','webm']," +
        "audioCapabilities:[{contentType:c}]}]);r[n]=true}catch(e){r[n]=String(e.name)}}" +
        "console.log('CRUMB_MEDIA:'+JSON.stringify(r))})()"

    // Вернулись из «Параметров Windows» — проверить, стал ли Crumb браузером по умолчанию.
    onActiveChanged: if (active) DefaultBrowser.recheck()
    property bool defaultBannerDismissed: false
    onVisibilityChanged: WindowEffects.roundCorners(win, !maximized)

    // ====================================================== вкладка-делегат
    component TabDelegate: TabItem {
        id: td
        required property int index
        required property int groupId
        readonly property var page: { pages.count; return win.pageAt(index) }
        readonly property int gIdx: { tabs.count; groups.count; return win.groupIndex(groupId) }
        readonly property var group: Settings.tabsGroups && gIdx >= 0 ? groups.get(gIdx) : null

        title: page ? page.title : ""
        favicon: page ? page.favicon : ""
        glyph: page ? page.glyph : ""
        loading: page ? page.loading : false
        active: index === win.currentIndex
        inGroup: group !== null
        groupStart: inGroup && (index === 0 || tabs.get(index - 1).groupId !== groupId)
        groupName: group ? group.name : ""
        groupCollapsed: group ? group.collapsed : false
        groupColor: { Theme.dark; return group ? Theme.groupColor(group.colorIndex) : "transparent" }

        onActivated: win.activate(index)
        onCloseRequested: win.closeTab(index)
        onContextMenuRequested: { tabMenu.tabIndex = index; tabMenu.popup() }
        onGroupClicked: if (gIdx >= 0) groups.setProperty(gIdx, "collapsed", !group.collapsed)
        onGroupMenuRequested: { groupMenu.gid = groupId; groupMenu.popup() }
    }

    // ===================================================== заголовок окна
    Item {
        id: titleBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: win.edge + 4
        anchors.rightMargin: win.edge
        anchors.topMargin: win.maximized ? 2 : 4
        height: win.sideTabs ? 40 : Math.max(42, Settings.tabsHeight + 2)

        Item {
            anchors.fill: parent
            DragHandler { target: null; onActiveChanged: if (active) win.startSystemMove() }
            TapHandler { onDoubleTapped: win.maximized ? win.showNormal() : win.showMaximized() }
        }

        // Логотип и название (при боковых вкладках)
        Row {
            visible: win.sideTabs
            anchors.verticalCenter: parent.verticalCenter
            x: 8
            spacing: 10
            Image { source: "qrc:/crumb/icon.png"; width: 22; height: 22; sourceSize: Qt.size(44, 44); mipmap: true; anchors.verticalCenter: parent.verticalCenter }
            CText { text: win.title; size: 13; color: Theme.onSurfaceVariant; elide: Text.ElideRight; width: titleBar.width - 260; anchors.verticalCenter: parent.verticalCenter }
        }

        // Горизонтальные вкладки
        Flickable {
            id: tabFlick
            visible: !win.sideTabs
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: Math.min(tabRow.width, tabsArea)
            readonly property real tabsArea: parent.width - newTabBtn.width - windowButtons.width - 24
            contentWidth: tabRow.width
            interactive: tabRow.width > width
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            WheelHandler {
                onWheel: (e) => tabFlick.contentX = Math.max(0, Math.min(tabFlick.contentWidth - tabFlick.width,
                                                                         tabFlick.contentX - e.angleDelta.y))
            }
            Row {
                id: tabRow
                height: parent.height
                spacing: 2
                Repeater {
                    model: win.sideTabs ? 0 : tabs
                    TabDelegate {
                        tabWidth: {
                            const n = Math.max(1, tabs.count)
                            return Math.max(72, Math.min(232, (tabFlick.tabsArea - groups.count * 60) / n - 2))
                        }
                    }
                }
            }
        }

        CookieButton {
            id: newTabBtn
            visible: !win.sideTabs
            anchors.left: tabFlick.right
            anchors.leftMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 2
            size: 30
            icon: ""
            toolTip: qsTr("Новая вкладка (%1)").arg(Settings.shortcutNewTab)
            onClicked: win.newHomeTab()
        }

        Row {
            id: windowButtons
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            CookieButton { size: 30; variant: "surface"; icon: ""; toolTip: qsTr("Свернуть"); onClicked: win.showMinimized() }
            CookieButton {
                size: 30; variant: "surface"
                icon: win.maximized ? "" : ""
                toolTip: win.maximized ? qsTr("Восстановить") : qsTr("Развернуть")
                onClicked: win.maximized ? win.showNormal() : win.showMaximized()
            }
            CookieButton {
                size: 30; variant: "surface"; icon: ""
                color: hovered ? Theme.error : Theme.surfaceContainerHighest
                contentColor: hovered ? Theme.onError : Theme.onSurfaceVariant
                toolTip: qsTr("Закрыть")
                onClicked: win.close()
            }
        }
    }

    // ============================================ боковая полоса вкладок
    Item {
        id: sidebar
        visible: win.sideTabs
        anchors.top: titleBar.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: win.edge
        anchors.bottomMargin: win.edge
        width: win.sideTabs ? 248 : 0

        CookieButton {
            id: sideNewTab
            x: 8
            y: 4
            size: 32
            icon: ""
            text: qsTr("Новая вкладка")
            onClicked: win.newHomeTab()
        }
        Flickable {
            anchors.top: sideNewTab.bottom
            anchors.topMargin: 8
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            contentHeight: sideCol.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            Column {
                id: sideCol
                width: parent.width - 8
                spacing: 2
                Repeater {
                    model: win.sideTabs ? tabs : 0
                    TabDelegate { vertical: true }
                }
            }
        }
    }

    // ================================================ панель и страницы
    Rectangle {
        id: card
        anchors.top: titleBar.bottom
        anchors.left: win.sideTabs ? sidebar.right : parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: win.sideTabs ? 4 : win.edge
        anchors.rightMargin: win.edge
        anchors.bottomMargin: win.edge
        radius: win.maximized && !win.sideTabs ? 0 : 14
        color: Theme.surface

        Item {
            id: toolbar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: win.compact ? 46 : 58

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 6

                // Кнопки до адресной строки — в порядке из настроек
                Repeater {
                    model: (Settings.toolbarButtons || []).filter(id => ["back", "forward", "reload", "home", "newTab"].indexOf(id) >= 0)
                    ToolbarButton { required property string modelData; buttonId: modelData }
                }

                AddressBar {
                    id: toolbarAddress
                    Layout.fillWidth: true
                    Layout.leftMargin: 6
                    Layout.rightMargin: 6
                    implicitHeight: win.compact ? 36 : 44
                    url: win.currentPage ? win.currentPage.displayUrl : ""
                    secure: win.currentPage ? win.currentPage.secure : false
                    loading: win.currentPage ? win.currentPage.loading : false
                    onEditingStarted: if (win.currentPage) win.currentPage.releasePageFocus()
                    onEditingFinished: suggest.hide()
                    onTyped: (t) => win.updateSuggestions(t)
                    onArrow: (step) => suggest.move(step)
                    onSubmitted: (text) => {
                        const s = suggest.shown && suggest.current >= 0 ? suggest.items[suggest.current] : null
                        suggest.hide()
                        win.currentPage.navigate(s ? win.suggestionTarget(s) : text)
                    }
                }

                // Закреплённые расширения — печеньки с их значками
                Repeater {
                    model: Extensions.items.filter(e => e.enabled && e.hasAction && (Settings.extPinned || []).indexOf(e.id) >= 0)
                    CookieButton {
                        id: extBtn
                        required property var modelData
                        size: win.compact ? 32 : 40
                        variant: "surface"
                        iconSource: modelData.icon
                        letter: (modelData.name || "?")[0]
                        toolTip: modelData.name
                        onClicked: win.openExtension(modelData, extBtn)
                    }
                }

                // Кнопки после адресной строки
                Repeater {
                    model: (Settings.toolbarButtons || []).filter(id => ["extensions", "history", "passwords", "rewind", "downloads", "theme", "settings"].indexOf(id) >= 0)
                    ToolbarButton { required property string modelData; buttonId: modelData }
                }
            }

            // Полоса загрузки по нижнему краю панели
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.leftMargin: 14
                height: 3
                radius: 1.5
                color: Theme.primary
                readonly property real p: win.currentPage ? win.currentPage.progress : 0
                width: (parent.width - 28) * p
                opacity: win.currentPage && win.currentPage.loading ? 1 : 0
                Behavior on width { enabled: Prefs.animations; SpringAnimation { spring: Prefs.spring(3); damping: 0.4; epsilon: 0.5 } }
                Behavior on opacity { NumberAnimation { duration: Prefs.dur(300) } }
            }
        }

        // Предложение сделать Crumb браузером по умолчанию
        Rectangle {
            id: defaultBanner
            readonly property bool shown: Settings.askDefaultBrowser && !DefaultBrowser.isDefault && !win.defaultBannerDismissed
            anchors.top: toolbar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            height: shown ? 56 : 0
            visible: height > 1
            clip: true
            radius: 18
            color: Theme.secondaryContainer
            Behavior on height { enabled: Prefs.animations; SpringAnimation { spring: Prefs.spring(3); damping: 0.35 } }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 12
                Image { source: "qrc:/crumb/icon.png"; Layout.preferredWidth: 32; Layout.preferredHeight: 32; sourceSize: Qt.size(64, 64); mipmap: true }
                CText {
                    Layout.fillWidth: true
                    text: qsTr("Сделать Crumb браузером по умолчанию? Ссылки из других программ будут открываться здесь.")
                    color: Theme.onSecondaryContainer
                    elide: Text.ElideRight
                }
                CookieButton {
                    size: 32; variant: "surface"; icon: "\uE711"
                    text: qsTr("Больше не спрашивать")
                    labelColor: Theme.onSecondaryContainer
                    onClicked: Settings.askDefaultBrowser = false
                }
                CookieButton {
                    size: 32; variant: "surface"; icon: "\uE916"
                    text: qsTr("Не сейчас")
                    labelColor: Theme.onSecondaryContainer
                    onClicked: win.defaultBannerDismissed = true
                }
                CookieButton {
                    size: 32; variant: "filled"; icon: "\uE73E"
                    text: qsTr("Сделать")
                    labelColor: Theme.onSecondaryContainer
                    onClicked: DefaultBrowser.openSystemSettings()
                }
            }
        }

        // Страницы: по одной на вкладку, видна только текущая.
        Item {
            id: pageArea
            anchors.top: defaultBanner.bottom
            anchors.topMargin: defaultBanner.visible ? 6 : 0
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 6
            anchors.rightMargin: 6
            anchors.bottomMargin: 6
            clip: true

            Repeater {
                id: pages
                model: tabs
                delegate: Item {
                    id: pageItem
                    required property int index
                    required property string kind
                    required property string url
                    required property bool webStarted
                    required property bool fromHome
                    required property int groupId
                    required property bool lazy
                    required property string savedTitle

                    anchors.fill: parent
                    onCurrentUrlChanged: win.scheduleSave()
                    onTitleChanged: win.scheduleSave()
                    readonly property bool current: index === win.currentIndex
                    visible: current
                    onCurrentChanged: if (current && !isWeb && Prefs.animations) pageEnter.restart()
                    onKindChanged: if (current && !isWeb && Prefs.animations) pageEnter.restart()
                    transformOrigin: Item.Top
                    ParallelAnimation {
                        id: pageEnter
                        NumberAnimation { target: pageItem; property: "opacity"; from: 0; to: 1; duration: Prefs.dur(200); easing.type: Easing.OutQuad }
                        NumberAnimation { target: pageItem; property: "scale"; from: 0.975; to: 1; duration: Prefs.dur(340); easing.type: Easing.OutCubic }
                    }

                    readonly property var view: webLoader.item
                    readonly property bool isWeb: kind === "web"
                    readonly property bool isInternal: kind === "settings" || kind === "downloads" || kind === "extensions"
                                                       || kind === "history" || kind === "passwords"
                    readonly property string title: kind === "home" ? qsTr("Новая вкладка")
                                                  : kind === "settings" ? qsTr("Настройки")
                                                  : kind === "downloads" ? qsTr("Загрузки")
                                                  : kind === "extensions" ? qsTr("Расширения")
                                                  : kind === "history" ? qsTr("История")
                                                  : kind === "passwords" ? qsTr("Пароли")
                                                  : (view && view.title.length ? view.title : (savedTitle.length ? savedTitle : url))
                    readonly property string favicon: kind === "home" ? "qrc:/crumb/icon.png"
                                                    : (isWeb && view ? view.favicon : (isWeb && lazy ? History.iconFor(url) : ""))
                    readonly property string glyph: kind === "settings" ? "\uE713" : kind === "downloads" ? "\uE896"
                                                  : kind === "extensions" ? "\uEA86" : kind === "history" ? "\uE823"
                                                  : kind === "passwords" ? "\uE8D7" : ""
                    readonly property bool loading: isWeb && view ? view.loading : false
                    readonly property real progress: isWeb && view ? view.progress : 0
                    readonly property bool secure: isWeb && view ? view.secure : false
                    readonly property string displayUrl: isInternal ? "crumb://" + kind
                                                       : kind === "home" ? "" : (view ? view.displayUrl : url)
                    readonly property string currentUrl: isWeb && view ? view.url : (isInternal ? "crumb://" + kind : (isWeb ? url : ""))
                    readonly property bool canGoBack: isWeb ? (view !== null && (view.canGoBack || fromHome)) : false
                    readonly property bool canGoForward: isWeb ? (view !== null && view.canGoForward) : (kind === "home" && webStarted)

                    function setKind(k) { tabs.setProperty(index, "kind", k) }
                    function navigate(text) {
                        const t = text.trim()
                        if (!t.length) return
                        if (lazy && !win.internalKind(t)) { tabs.setProperty(index, "lazy", false); tabs.setProperty(index, "url", t); tabs.setProperty(index, "webStarted", true); return }
                        if (win.internalKind(t)) {
                            // Пустая новая вкладка сама становится страницей Crumb (если такой ещё нет).
                            const k = win.internalKind(t)
                            let exists = false
                            for (let i = 0; i < tabs.count; ++i) if (tabs.get(i).kind === k) exists = true
                            if (!exists && kind === "home" && !webStarted) setKind(k); else win.openPage(k)
                            toolbarAddress.cancelEditing()
                            return
                        }
                        if (t === "crumb://home" || t === "crumb://newtab") { setKind("home"); return }
                        const wasHome = kind === "home"
                        if (!webStarted) {
                            tabs.setProperty(index, "url", t)
                            tabs.setProperty(index, "fromHome", wasHome)
                            tabs.setProperty(index, "webStarted", true)
                            setKind("web")
                        } else {
                            setKind("web")
                            if (view) view.navigate(t)
                        }
                    }
                    function goBack() {
                        if (!isWeb || !view) return
                        if (view.canGoBack) view.goBack()
                        else if (fromHome) setKind("home")
                    }
                    function goForward() {
                        if (kind === "home" && webStarted) setKind("web")
                        else if (view) view.goForward()
                    }
                    function goHome() { setKind("home"); tabs.setProperty(index, "fromHome", webStarted) }
                    function reload() { if (isWeb && view) view.reload() }
                    function stop() { if (view) view.stop() }
                    function releasePageFocus() { if (isWeb && view) view.releasePageFocus() }

                    Loader {
                        id: webLoader
                        anchors.fill: parent
                        active: pageItem.webStarted
                        visible: pageItem.isWeb
                        sourceComponent: BrowserView {
                            id: bv
                            initialUrl: pageItem.url
                            onAddressFocusRequested: toolbarAddress.focusAddress()
                            onPopupRequested: (target, background) => win.newTab("web", target, pageItem.index + 1, !background, pageItem.groupId)
                            onChromeCommand: (id) => win.command(id, pageItem.index)
                            onShortcutTriggered: (name) => win.action(name, pageItem.index)
                            onPageClosed: win.closeTab(pageItem.index)
                            onUrlChanged: {
                                const k = win.internalKind(url)
                                if (!k) return
                                if (canGoBack) goBack(); else Qt.callLater(win.closeTab, pageItem.index)
                                win.openPage(k)
                            }
                            onLoadingStateChanged: {
                                if (loading) { if (fillPanel.view === bv) fillPanel.hide(); return }
                                // Скрипт входа на сайтах: предложить сохранить пароль и подставить сохранённый.
                                if ((Settings.passwordsOffer || Settings.passwordsAutofill) && /^https?:/.test(url))
                                    executeJavaScript(Passwords.captureJs())
                                if (url.indexOf("crumb-mediatest") >= 0) executeJavaScript(win.mediaTestJs)
                            }
                            onConsoleMessage: (m) => {
                                if (m.startsWith("CRUMB_PW_")) win.passwordMessage(bv, m)
                                else if (m.startsWith("CRUMB_MEDIA:")) console.log(m)
                            }
                        }
                    }
                    Loader {
                        anchors.fill: parent
                        active: pageItem.kind === "home"
                        sourceComponent: HomePage {
                            onNavigate: (t) => pageItem.navigate(t)
                            onOpenSettings: win.openSettings()
                        }
                    }
                    Loader {
                        anchors.fill: parent
                        active: pageItem.kind === "settings"
                        sourceComponent: SettingsPage {
                            onPickColorFromScreen: eyedropper.start()
                        }
                    }
                    Loader {
                        anchors.fill: parent
                        active: pageItem.kind === "downloads"
                        sourceComponent: DownloadsPage {}
                    }
                    Loader {
                        anchors.fill: parent
                        active: pageItem.kind === "history"
                        sourceComponent: HistoryPage {
                            closedTabs: win.closedTabs
                            onOpenUrl: (u) => win.openUrl(u, pageItem.index + 1)
                            onReopenClosed: (k) => win.reopenClosedAt(k)
                        }
                    }
                    Loader {
                        anchors.fill: parent
                        active: pageItem.kind === "passwords"
                        sourceComponent: PasswordsPage {
                            onOpenUrl: (u) => win.openUrl(u, pageItem.index + 1)
                        }
                    }
                    Loader {
                        anchors.fill: parent
                        active: pageItem.kind === "extensions"
                        sourceComponent: ExtensionsPage {
                            onOpenUrl: (u) => win.openUrl(u, pageItem.index + 1)
                        }
                    }
                }
            }
        }
    }

    // Кнопка панели по её id из настроек
    component ToolbarButton: CookieButton {
        id: tbb
        property string buttonId
        ProgressRing {
            anchors.centerIn: parent
            width: tbb.realSize + 8
            height: width
            visible: tbb.buttonId === "downloads" && Downloads.activeCount > 0
            value: Downloads.activeProgress
        }
        // Точка «что-то изменилось» на кнопке Rewind
        Rectangle {
            visible: tbb.buttonId === "rewind" && Rewind.unseenWatches > 0
            anchors.right: parent.right; anchors.top: parent.top
            width: 12; height: 12; radius: 6
            color: Theme.error; border.color: Theme.surface; border.width: 2
        }
        readonly property var p: win.currentPage
        Component.onCompleted: if (buttonId === "downloads") win.downloadsButton = tbb
        // «Подпрыгнуть» — началась или закончилась загрузка; «покачаться» — сработала подписка Rewind
        function bounce() { if (Prefs.animations) bounceAnim.restart() }
        function wiggle() { if (Prefs.animations) wiggleAnim.restart() }
        transform: Translate { id: hop; y: 0 }
        SequentialAnimation {
            id: bounceAnim
            NumberAnimation { target: hop; property: "y"; to: -9; duration: Prefs.dur(140); easing.type: Easing.OutQuad }
            NumberAnimation { target: hop; property: "y"; to: 0; duration: Prefs.dur(520); easing.type: Easing.OutBounce }
        }
        SequentialAnimation {
            id: wiggleAnim
            loops: 2
            RotationAnimation { target: tbb; to: 14; duration: Prefs.dur(90) }
            RotationAnimation { target: tbb; to: -14; duration: Prefs.dur(160) }
            RotationAnimation { target: tbb; to: 0; duration: Prefs.dur(90) }
        }
        property int lastActive: 0
        Connections {
            target: Downloads
            enabled: tbb.buttonId === "downloads"
            function onRowsInserted() { tbb.bounce() }
            function onStatsChanged() {
                if (Downloads.activeCount < tbb.lastActive) tbb.bounce()   // что-то докачалось
                tbb.lastActive = Downloads.activeCount
            }
        }
        Connections {
            target: Rewind
            enabled: tbb.buttonId === "rewind"
            function onWatchTriggered() { tbb.wiggle() }
            function onWatchAdded() { tbb.bounce() }
        }
        Component.onDestruction: if (win.downloadsButton === tbb) win.downloadsButton = null
        size: win.compact ? 32 : 40
        icon: ({ back: "\uE72B", forward: "\uE72A", home: "\uE80F", newTab: "\uE710", theme: "\uE790", settings: "\uE713",
                 downloads: "\uE896", extensions: "\uEA86", rewind: "\uE81C", history: "\uE823", passwords: "\uE8D7",
                 reload: p && p.loading ? "" : "" })[buttonId] || ""
        variant: buttonId === "theme" ? "filled" : "tonal"
        toolTip: ({ back: qsTr("Назад (%1)").arg(Settings.shortcutBack), forward: qsTr("Вперёд (%1)").arg(Settings.shortcutForward),
                    reload: p && p.loading ? qsTr("Остановить") : qsTr("Обновить (%1)").arg(Settings.shortcutReload),
                    home: qsTr("Домашняя страница (%1)").arg(Settings.shortcutHome), newTab: qsTr("Новая вкладка (%1)").arg(Settings.shortcutNewTab),
                    theme: qsTr("Тема и цвета"), settings: qsTr("Настройки (%1)").arg(Settings.shortcutSettings),
                    downloads: Downloads.activeCount > 0 ? qsTr("Загрузки: %1 в процессе").arg(Downloads.activeCount)
                                                         : qsTr("Загрузки (%1)").arg(Settings.shortcutDownloads),
                    extensions: qsTr("Расширения"),
                    history: qsTr("История (%1)").arg(Settings.shortcutHistory), passwords: qsTr("Пароли"),
                    rewind: Rewind.unseenWatches > 0 ? qsTr("Rewind: изменилось %1").arg(Rewind.unseenWatches)
                                                     : qsTr("Rewind — машина времени (%1)").arg(Settings.shortcutRewind) })[buttonId] || ""
        enabled: buttonId === "back" ? (p ? p.canGoBack : false)
               : buttonId === "forward" ? (p ? p.canGoForward : false)
               : buttonId === "reload" ? (p ? p.isWeb : false) : true
        onClicked: {
            switch (buttonId) {
            case "back": p.goBack(); break
            case "forward": p.goForward(); break
            case "reload": p.loading ? p.stop() : p.reload(); break
            case "home": p.goHome(); break
            case "newTab": win.newHomeTab(); break
            case "settings": win.openSettings(); break
            case "downloads": win.openDownloads(true); break
            case "rewind": win.openRewind(this); break
            case "history": win.openPage("history"); break
            case "passwords": win.openPage("passwords"); break
            case "extensions":
                extMenu.parent = this
                extMenu.x = width - extMenu.width
                extMenu.y = height + 8
                extMenu.opened ? extMenu.close() : extMenu.open()
                break
            case "theme":
                themePopup.parent = this
                themePopup.x = width - themePopup.width
                themePopup.y = height + 8
                themePopup.opened ? themePopup.close() : themePopup.open()
                break
            }
        }
    }

    // Кольцо прогресса вокруг кнопки загрузок
    component ProgressRing: Item {
        id: ring
        property real value: -1      // 0…1; -1 — неизвестно (крутится)
        property real spinAngle: 0
        NumberAnimation on spinAngle { from: 0; to: 360; duration: 1200; loops: Animation.Infinite; running: ring.visible && ring.value < 0 }
        Canvas {
            anchors.fill: parent
            property real v: ring.value
            property real a: ring.spinAngle
            property color c: Theme.primary
            onVChanged: requestPaint()
            onAChanged: if (ring.value < 0) requestPaint()
            onCChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                ctx.lineWidth = 3
                ctx.lineCap = "round"
                ctx.strokeStyle = c
                const r = width / 2 - 2
                ctx.beginPath()
                if (ring.value < 0) {
                    const st = (ring.spinAngle - 90) * Math.PI / 180
                    ctx.arc(width / 2, height / 2, r, st, st + Math.PI * 0.6)
                } else {
                    ctx.arc(width / 2, height / 2, r, -Math.PI / 2, -Math.PI / 2 + Math.PI * 2 * Math.max(0.02, ring.value))
                }
                ctx.stroke()
            }
        }
    }

    // «Банка с печеньем»: все включённые расширения, закрепление на панели.
    CookiePopup {
        id: extMenu
        width: 320
        padding: 10
        background: Rectangle { color: Theme.surfaceContainer; radius: 22; border.color: Theme.outlineVariant }
        contentItem: Column {
            spacing: 4
            CText { text: qsTr("Расширения"); size: 16; font.weight: Font.DemiBold; leftPadding: 8; topPadding: 4; bottomPadding: 6 }
            CText {
                visible: Extensions.items.filter(e => e.enabled).length === 0
                text: qsTr("Нет включённых расширений")
                color: Theme.onSurfaceVariant
                leftPadding: 8
                bottomPadding: 6
            }
            Repeater {
                model: Extensions.items.filter(e => e.enabled)
                Rectangle {
                    id: extRow
                    required property var modelData
                    readonly property bool pinned: (Settings.extPinned || []).indexOf(modelData.id) >= 0
                    width: 300
                    height: 46
                    radius: 14
                    color: rowHover.hovered ? Qt.alpha(Theme.onSurface, 0.08) : "transparent"
                    CookieShape {
                        x: 6
                        anchors.verticalCenter: parent.verticalCenter
                        width: 34; height: 34
                        color: Theme.surfaceContainerHighest
                        Image {
                            anchors.centerIn: parent
                            width: 20; height: 20
                            source: extRow.modelData.icon
                            sourceSize: Qt.size(40, 40)
                            mipmap: true
                        }
                    }
                    CText {
                        x: 50
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 100
                        text: extRow.modelData.name
                        elide: Text.ElideRight
                    }
                    HoverHandler { id: rowHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: { extMenu.close(); win.openExtension(extRow.modelData, extMenuAnchor()) } }
                    CookieButton {
                        anchors.right: parent.right
                        anchors.rightMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        visible: extRow.modelData.hasAction === true
                        size: 30
                        icon: "\uE718"
                        checked: extRow.pinned
                        variant: "surface"
                        toolTip: extRow.pinned ? qsTr("Открепить от панели") : qsTr("Закрепить на панели")
                        onClicked: {
                            const list = (Settings.extPinned || []).filter(x => x !== extRow.modelData.id)
                            if (!extRow.pinned) list.push(extRow.modelData.id)
                            Settings.extPinned = list
                        }
                    }
                }
            }
            CookieButton {
                icon: "\uE713"
                variant: "surface"
                size: 32
                text: qsTr("Управление расширениями")
                onClicked: { extMenu.close(); win.openPage("extensions") }
            }
        }
    }
    function extMenuAnchor() { return extMenu.parent ? extMenu.parent : toolbarAddress }

    ExtensionPopup { id: extPopup }

    // ----------------------------------------------------------- Загрузки
    property Item downloadsButton: null
    DownloadsPopup {
        id: downloadsPopup
        onOpenFullPage: win.openPage("downloads")
    }
    function openDownloads(toggle) {
        if (downloadsPopup.opened) { if (toggle) downloadsPopup.close(); return }
        const a = downloadsButton && downloadsButton.visible ? downloadsButton : toolbarAddress
        downloadsPopup.parent = a
        downloadsPopup.x = a.width - downloadsPopup.width
        downloadsPopup.y = a.height + 8
        downloadsPopup.open()
    }
    // Началась загрузка — показываем окошко, как Chrome/Edge.
    Connections {
        target: Downloads
        function onRowsInserted() { if (win.active || win.visible) win.openDownloads(false) }
    }

    // ------------------------------------------------------------ Rewind
    RewindPopup {
        id: rewindPopup
        onOpenUrl: (u) => win.openUrl(u, win.currentIndex + 1)
        onPickFragment: {
            const p = win.currentPage
            if (p && p.view) { p.view.focusPage(); p.view.executeJavaScript(Rewind.pickerJs()) }
        }
    }
    function openRewind(anchor) {
        if (rewindPopup.opened) { rewindPopup.close(); return }
        const p = win.currentPage
        rewindPopup.pageUrl = p && p.isWeb && p.view ? p.view.url : ""
        rewindPopup.pageTitle = p && p.view ? p.view.title : ""
        const a = anchor || toolbarAddress
        rewindPopup.parent = a
        rewindPopup.x = a.width - rewindPopup.width
        rewindPopup.y = a.height + 8
        rewindPopup.open()
    }
    Connections {
        target: Sync
        function onOpenUrlRequested(url) { win.openUrl(url, win.currentIndex + 1) }
    }
    Connections {
        target: Rewind
        function onWatchAdded(label) { toast.show(qsTr("Слежу за «%1» — сообщу, когда изменится").arg(label)) }
    }
    // Короткое уведомление внизу окна
    Rectangle {
        id: toast
        function show(t) { toastText.text = t; opacity = 1; toastTimer.restart() }
        z: 100
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom; anchors.bottomMargin: 28
        width: Math.min(toastText.implicitWidth + 40, parent.width - 40); height: 44; radius: 22
        color: Theme.inverseSurface
        opacity: 0
        visible: opacity > 0
        transform: Translate { y: (1 - toast.opacity) * 24 }
        Behavior on opacity { NumberAnimation { duration: Prefs.dur(260); easing.type: Easing.OutCubic } }
        CText { id: toastText; anchors.centerIn: parent; width: Math.min(implicitWidth, parent.width - 40); elide: Text.ElideRight; color: Theme.inverseOnSurface; size: 14 }
        Timer { id: toastTimer; interval: 3500; onTriggered: toast.opacity = 0 }
    }
    // Старые профили: добавить кнопку Rewind на панель один раз.
    Timer {
        interval: 50; running: true
        onTriggered: {
            if (Settings.toolbarRewindAdded) return
            const list = (Settings.toolbarButtons || []).slice()
            if (list.indexOf("rewind") < 0) {
                const at = list.indexOf("downloads")
                list.splice(at >= 0 ? at : list.length, 0, "rewind")
                Settings.toolbarButtons = list
            }
            Settings.toolbarRewindAdded = true
        }
    }

    ThemePopup {
        id: themePopup
        onOpenSettings: { close(); win.openSettings() }
    }

    EyedropperWindow {
        id: eyedropper
        onPicked: (c) => Theme.customColor = c
    }

    // ================================================= сеанс: вкладки
    // Вкладки и группы сохраняются через полторы секунды после любого изменения
    // (и при закрытии окна) — после перезапуска или сбоя всё вернётся.
    property bool sessionReady: false
    property bool sessionFrozen: false
    function scheduleSave() { if (sessionReady && !sessionFrozen) sessionTimer.restart() }
    Timer { id: sessionTimer; interval: 1500; onTriggered: win.saveSession() }
    Connections {
        target: tabs
        function onCountChanged() { win.scheduleSave() }
        function onDataChanged() { win.scheduleSave() }
    }
    Connections {
        target: groups
        function onCountChanged() { win.scheduleSave() }
        function onDataChanged() { win.scheduleSave() }
    }
    onClosedTabsChanged: scheduleSave()

    function saveSession() {
        if (!sessionReady || sessionFrozen) return
        const t = []
        for (let i = 0; i < tabs.count; ++i) {
            const m = tabs.get(i), p = pageAt(i)
            const url = m.kind === "web" ? ((p && p.currentUrl) || m.url) : ""
            t.push({ kind: m.kind, url: url, title: p ? p.title : m.savedTitle, groupId: m.groupId })
        }
        const g = []
        for (let k = 0; k < groups.count; ++k) {
            const x = groups.get(k)
            g.push({ gid: x.gid, name: x.name, colorIndex: x.colorIndex, collapsed: x.collapsed })
        }
        const s = JSON.stringify({ v: 1, current: currentIndex, tabs: t, groups: g, closed: closedTabs.slice(-15) })
        if (s !== Settings.sessionState) Settings.sessionState = s
    }
    function restoreSession() {
        let s = null
        try { s = JSON.parse(Settings.sessionState || "null") } catch (e) { return false }
        if (!s || !s.tabs || !s.tabs.length) return false
        for (const g of (s.groups || [])) {
            groups.append({ gid: g.gid, name: g.name, colorIndex: g.colorIndex, collapsed: !!g.collapsed })
            nextGid = Math.max(nextGid, g.gid + 1)
        }
        const cur = Math.max(0, Math.min(s.current || 0, s.tabs.length - 1))
        for (let i = 0; i < s.tabs.length; ++i) {
            const x = s.tabs[i]
            const web = x.kind === "web" && !!x.url
            const kind = x.kind === "web" ? (web ? "web" : "home") : x.kind
            // Сайты загружаются, только когда вкладку откроют, — запуск остаётся быстрым.
            tabs.append({ uid: nextUid++, kind: kind, url: web ? x.url : "", webStarted: web && i === cur,
                          fromHome: false, groupId: groupExists(x.groupId) ? x.groupId : -1,
                          lazy: web && i !== cur, savedTitle: x.title || "" })
        }
        closedTabs = s.closed || []
        currentIndex = cur
        return true
    }
    // Восстановленная вкладка: сайт начинает загружаться при первом открытии.
    function wake(i) {
        if (i < 0 || i >= tabs.count || !tabs.get(i).lazy) return
        tabs.setProperty(i, "lazy", false)
        tabs.setProperty(i, "webStarted", true)
    }

    // ================================================ поиск по странице
    FindBar {
        id: findBar
        parent: pageArea
        x: pageArea.width - width - 10
        y: 8
        onClosedByUser: { const p = win.currentPage; if (p && p.view && p.isWeb) p.view.focusPage() }
    }
    function openFind() {
        const p = currentPage
        if (!p || !p.isWeb || !p.view) return
        p.releasePageFocus()
        findBar.openFor(p.view)
    }

    // ============================================ подсказки адресной строки
    SuggestList {
        id: suggest
        onPicked: (item) => { suggest.hide(); toolbarAddress.cancelEditing(); win.currentPage.navigate(win.suggestionTarget(item)) }
    }
    // «Поиск в интернете» — всегда поиск, даже если текст похож на адрес («127»).
    function suggestionTarget(s) {
        return s.kind === "search" ? Settings.searchTemplate().replace("%s", encodeURIComponent(s.url)) : s.url
    }
    function updateSuggestions(text) {
        const t = text.trim()
        if (!t.length) { suggest.hide(); return }
        const looksUrl = t.indexOf(" ") < 0 && (/^[a-z]+:\/\//i.test(t) || /^[^\s.]+(\.[^\s.]+)+(\/.*)?$/.test(t) || /^localhost(:\d+)?/.test(t))
        const first = looksUrl ? { kind: "go", title: t, text: qsTr("перейти"), url: t, icon: "" }
                               : { kind: "search", title: t, text: qsTr("поиск в интернете"), url: t, icon: "" }
        const hist = History.suggest(t, 6).map(h => ({ kind: "history", title: h.title || h.host,
                                                       text: h.url.replace(/^https?:\/\/(www\.)?/, "").replace(/\/$/, ""),
                                                       url: h.url, icon: h.icon }))
        suggest.items = [first].concat(hist.filter(h => h.url !== t))
        suggest.current = -1
        const a = toolbarAddress
        const p = a.mapToGlobal(0, a.height + 4)
        suggest.showAt(p.x - 2, p.y, a.width + 4, suggest.items.length * suggest.rowHeight + suggest.padding * 2 + 4)
    }

    // ===================================================== пароли на сайтах
    PasswordSavePopup {
        id: savePw
        onSaved: toast.show(qsTr("Пароль сохранён — Crumb подставит его при следующем входе"))
    }
    // Список сохранённых входов под полем логина/пароля на сайте.
    FloatingPanel {
        id: fillPanel
        property var view: null
        property var items: []
        Column {
            anchors.fill: parent
            Repeater {
                model: fillPanel.items
                Rectangle {
                    id: acc
                    required property var modelData
                    width: parent.width
                    height: 48
                    radius: 16
                    color: accHover.hovered ? Theme.secondaryContainer : "transparent"
                    HoverHandler { id: accHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler {
                        onTapped: {
                            const js = Passwords.fillJs(acc.modelData.id)
                            if (fillPanel.view && js.length) fillPanel.view.executeJavaScript(js)
                            fillPanel.hide()
                        }
                    }
                    CookieShape {
                        id: accIcon
                        x: 8
                        anchors.verticalCenter: parent.verticalCenter
                        width: 32; height: 32
                        color: Theme.primaryContainer
                        Text { anchors.centerIn: parent; text: ""; font.family: Theme.iconFont; font.pixelSize: 13; color: Theme.onPrimaryContainer }
                    }
                    Column {
                        anchors.left: accIcon.right; anchors.leftMargin: 12
                        anchors.right: parent.right; anchors.rightMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        CText { width: parent.width; text: acc.modelData.username || qsTr("Без имени пользователя"); size: 14; elide: Text.ElideRight }
                        CText { width: parent.width; text: "•••••••• · " + acc.modelData.site; size: 12; color: Theme.onSurfaceVariant; elide: Text.ElideRight }
                    }
                }
            }
            Rectangle {
                width: parent.width
                height: 40
                radius: 14
                color: mgrHover.hovered ? Qt.alpha(Theme.onSurface, 0.07) : "transparent"
                HoverHandler { id: mgrHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: { fillPanel.hide(); win.openPage("passwords") } }
                CText { anchors.left: parent.left; anchors.leftMargin: 14; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Управление паролями…"); size: 13; color: Theme.primary }
            }
        }
    }
    function passwordMessage(view, m) {
        const sep = m.indexOf(":")
        const kind = m.slice(0, sep)
        let d = {}
        try { d = JSON.parse(m.slice(sep + 1) || "{}") } catch (e) { return }
        const pageUrl = String(view.url)
        if (kind === "CRUMB_PW_SAVE") {
            // Только для главного документа этой вкладки.
            if (!d.o || !(pageUrl === d.o || pageUrl.startsWith(d.o + "/"))) return
            const r = Passwords.check(pageUrl, d.u || "", d.p || "")
            if (r !== "new" && r !== "update") return
            const a = toolbarAddress
            savePw.parent = a
            savePw.x = a.width - savePw.width
            savePw.y = a.height + 8
            savePw.offer(pageUrl, d.u || "", d.p || "", r)
        } else if (kind === "CRUMB_PW_FOCUS") {
            const p = currentPage
            if (!Settings.passwordsAutofill || !p || p.view !== view) return
            const list = Passwords.forSite(pageUrl)
            if (!list.length) { fillPanel.hide(); return }
            const k = (d.dpr || 1) / (win.screen ? win.screen.devicePixelRatio : 1)
            const g = view.mapToGlobal(d.x * k, (d.y + d.h) * k + 4)
            if (g.y > view.mapToGlobal(0, view.height).y - 40) return   // поле у нижнего края — не мешаем
            fillPanel.view = view
            fillPanel.items = list
            fillPanel.showAt(g.x - 4, g.y, Math.max(280, Math.min(400, d.w * k + 8)), list.length * 48 + 40 + fillPanel.padding * 2 + 4)
        } else if (kind === "CRUMB_PW_BLUR") {
            fillPanel.hide()
        }
    }

    // Плавающие окна прячутся, когда Crumb уходит на задний план, двигается или меняет размер.
    function hidePanels() { suggest.hide(); fillPanel.hide() }
    onXChanged: hidePanels()
    onYChanged: hidePanels()
    onWidthChanged: hidePanels()
    onHeightChanged: hidePanels()
    Timer {
        interval: 300
        repeat: true
        running: suggest.shown || fillPanel.shown
        onTriggered: if (!CefRuntime.isForeground(win)) win.hidePanels()
    }

    // ============================================== меню вкладки и группы
    ThemedMenu {
        id: tabMenu
        property int tabIndex: -1
        readonly property bool grouped: tabIndex >= 0 && tabIndex < tabs.count && tabs.get(tabIndex).groupId >= 0
        ThemedMenuItem { text: qsTr("Новая вкладка справа"); onTriggered: win.newTab("home", "", tabMenu.tabIndex + 1) }
        ThemedMenuItem { text: qsTr("Перезагрузить"); onTriggered: win.pageAt(tabMenu.tabIndex).reload() }
        ThemedMenuItem { text: qsTr("Дублировать"); onTriggered: win.duplicate(tabMenu.tabIndex) }
        ThemedMenuSeparator {}
        ThemedMenuItem { text: qsTr("Добавить в новую группу"); enabled: Settings.tabsGroups; onTriggered: win.addToNewGroup(tabMenu.tabIndex) }
        ThemedMenuItem { text: qsTr("Убрать из группы"); enabled: tabMenu.grouped; onTriggered: win.removeFromGroup(tabMenu.tabIndex) }
        ThemedMenuSeparator {}
        ThemedMenuItem { text: qsTr("Закрыть вкладку"); onTriggered: win.closeTab(tabMenu.tabIndex) }
        ThemedMenuItem { text: qsTr("Закрыть другие вкладки"); enabled: tabs.count > 1; onTriggered: win.closeOthers(tabMenu.tabIndex) }
        ThemedMenuItem { text: qsTr("Вернуть закрытую вкладку"); enabled: win.closedTabs.length > 0; onTriggered: win.reopenClosed() }
    }

    ThemedMenu {
        id: groupMenu
        property int gid: -1
        readonly property int gIdx: { groups.count; return win.groupIndex(gid) }
        ThemedMenuItem {
            text: groupMenu.gIdx >= 0 && groups.get(groupMenu.gIdx).collapsed ? qsTr("Развернуть группу") : qsTr("Свернуть группу")
            onTriggered: groups.setProperty(groupMenu.gIdx, "collapsed", !groups.get(groupMenu.gIdx).collapsed)
        }
        ThemedMenuItem { text: qsTr("Переименовать…"); onTriggered: renamePopup.openFor(groupMenu.gid) }
        ThemedMenuItem {
            text: qsTr("Другой цвет")
            onTriggered: groups.setProperty(groupMenu.gIdx, "colorIndex", (groups.get(groupMenu.gIdx).colorIndex + 1) % Theme.groupColorCount())
        }
        ThemedMenuItem { text: qsTr("Новая вкладка в группе"); onTriggered: win.newTab("home", "", -1, true, groupMenu.gid) }
        ThemedMenuSeparator {}
        ThemedMenuItem { text: qsTr("Разгруппировать"); onTriggered: win.ungroup(groupMenu.gid) }
        ThemedMenuItem { text: qsTr("Закрыть группу"); onTriggered: win.closeGroup(groupMenu.gid) }
    }

    // Переименование группы — маленькое окно поверх полосы вкладок.
    CookiePopup {
        id: renamePopup
        transformOrigin: Popup.TopLeft
        property int gid: -1
        x: 40
        y: titleBar.height + 4
        padding: 14
        background: Rectangle { color: Theme.surfaceContainerHigh; radius: 18; border.color: Theme.outlineVariant }

        function openFor(g) {
            gid = g
            const i = win.groupIndex(g)
            nameField.text = i >= 0 ? groups.get(i).name : ""
            if (win.currentPage) win.currentPage.releasePageFocus()
            open()
            nameField.forceActiveFocus()
            nameField.selectAll()
        }
        function apply() {
            const i = win.groupIndex(gid)
            if (i >= 0) groups.setProperty(i, "name", nameField.text.trim())
            close()
        }

        contentItem: RowLayout {
            spacing: 10
            Rectangle {
                Layout.preferredWidth: 220
                Layout.preferredHeight: 40
                radius: 20
                color: Theme.surfaceContainerLowest
                border.color: Theme.primary
                border.width: 2
                TextInput {
                    id: nameField
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    verticalAlignment: TextInput.AlignVCenter
                    color: Theme.onSurface
                    font.family: Prefs.font
                    font.pixelSize: Prefs.px(14)
                    selectByMouse: true
                    maximumLength: 40
                    selectionColor: Theme.primaryContainer
                    selectedTextColor: Theme.onPrimaryContainer
                    onAccepted: renamePopup.apply()
                    Keys.onEscapePressed: renamePopup.close()
                }
            }
            CookieButton { icon: ""; variant: "filled"; toolTip: qsTr("Готово"); onClicked: renamePopup.apply() }
        }
    }

    // ================================================ изменение размера окна
    component ResizeEdge: MouseArea {
        property int edges
        enabled: !win.maximized
        onPressed: win.startSystemResize(edges)
    }
    ResizeEdge { edges: Qt.LeftEdge;   cursorShape: Qt.SizeHorCursor; width: 6; anchors { left: parent.left; top: parent.top; bottom: parent.bottom; topMargin: 12; bottomMargin: 12 } }
    ResizeEdge { edges: Qt.RightEdge;  cursorShape: Qt.SizeHorCursor; width: 6; anchors { right: parent.right; top: parent.top; bottom: parent.bottom; topMargin: 12; bottomMargin: 12 } }
    ResizeEdge { edges: Qt.TopEdge;    cursorShape: Qt.SizeVerCursor; height: 5; anchors { top: parent.top; left: parent.left; right: parent.right; leftMargin: 12; rightMargin: 12 } }
    ResizeEdge { edges: Qt.BottomEdge; cursorShape: Qt.SizeVerCursor; height: 6; anchors { bottom: parent.bottom; left: parent.left; right: parent.right; leftMargin: 12; rightMargin: 12 } }
    ResizeEdge { edges: Qt.LeftEdge | Qt.TopEdge;     cursorShape: Qt.SizeFDiagCursor; width: 12; height: 12; anchors { left: parent.left; top: parent.top } }
    ResizeEdge { edges: Qt.RightEdge | Qt.TopEdge;    cursorShape: Qt.SizeBDiagCursor; width: 12; height: 12; anchors { right: parent.right; top: parent.top } }
    ResizeEdge { edges: Qt.LeftEdge | Qt.BottomEdge;  cursorShape: Qt.SizeBDiagCursor; width: 12; height: 12; anchors { left: parent.left; bottom: parent.bottom } }
    ResizeEdge { edges: Qt.RightEdge | Qt.BottomEdge; cursorShape: Qt.SizeFDiagCursor; width: 12; height: 12; anchors { right: parent.right; bottom: parent.bottom } }

    // ===================================== горячие клавиши (из настроек)
    // Пока фокус в странице, те же сочетания ловит BrowserClient.
    component ActionShortcut: Shortcut {
        property string name
        sequence: Settings["shortcut" + name.charAt(0).toUpperCase() + name.slice(1)] || ""
        enabled: sequence.length > 0
        onActivated: win.action(name)
    }
    ActionShortcut { name: "newTab" }
    ActionShortcut { name: "closeTab" }
    ActionShortcut { name: "reopenTab" }
    ActionShortcut { name: "nextTab" }
    ActionShortcut { name: "prevTab" }
    ActionShortcut { name: "address" }
    ActionShortcut { name: "reload" }
    ActionShortcut { name: "back" }
    ActionShortcut { name: "forward" }
    ActionShortcut { name: "home" }
    ActionShortcut { name: "settings" }
    ActionShortcut { name: "downloads" }
    ActionShortcut { name: "rewind" }
    ActionShortcut { name: "history" }
    ActionShortcut { name: "find" }
    Shortcut { sequence: "F3"; onActivated: findBar.opened ? findBar.next() : win.openFind() }
    Shortcut { sequence: "Shift+F3"; onActivated: findBar.opened ? findBar.prev() : win.openFind() }
    // Неизменяемые, как в Chrome
    Shortcut { sequences: ["Alt+D", "F6"]; onActivated: toolbarAddress.focusAddress() }
    Shortcut { sequence: "Ctrl+F4"; onActivated: win.closeTab(win.currentIndex) }
    Shortcut { sequence: "Ctrl+9"; onActivated: win.activate(tabs.count - 1) }
    Repeater {
        model: 8
        Item {
            required property int index
            Shortcut { sequence: "Ctrl+" + (index + 1); onActivated: win.activate(index) }
        }
    }
    Shortcut {
        sequence: "Esc"
        enabled: win.currentPage !== null && win.currentPage.loading && !toolbarAddress.editing
        onActivated: win.currentPage.stop()
    }

    // Мост к расширениям Chromium — когда CEF готов.
    Connections {
        target: CefRuntime
        function onReadyChanged() { if (CefRuntime.ready) Extensions.start(win) }
        // Окно, которое открыл сам Chromium (например, расширение), — новой вкладкой.
        function onStrayUrlRequested(url) { win.openUrl(url, win.currentIndex + 1) }
    }
    Timer { interval: 1500; running: true; onTriggered: if (CefRuntime.ready) Extensions.start(win) }

    // Повторный запуск Crumb.exe с адресом — открываем его новой вкладкой.
    Connections {
        target: CefRuntime
        function onRelaunchRequested(args) {
            const urls = args.filter(a => !a.startsWith("-"))
            if (urls.length > 0) win.openUrl(urls[urls.length - 1])
            else win.newHomeTab()
            win.raise()
            win.requestActivate()
        }
    }
}
