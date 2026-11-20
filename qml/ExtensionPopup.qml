// Всплывающее окно расширения (action.default_popup) под его печенькой на панели.
// Размер подстраивается под содержимое; окно закрывается, когда теряет фокус.

import QtQuick
import Crumb
import Crumb.Ui

Window {
    id: popup
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: Theme.surfaceContainer
    width: 360
    height: 420

    property string url: ""

    function openAt(item, popupUrl) {
        if (visible) close()
        url = popupUrl
        width = 360
        height = 420
        const p = item.mapToGlobal(item.width, item.height + 6)
        x = Math.max(8, p.x - width)
        y = p.y
        loader.active = true
        show()
        requestActivate()
    }

    onActiveChanged: if (!active && visible) close()
    onVisibleChanged: if (!visible) { loader.active = false; url = "" }
    Component.onCompleted: WindowEffects.roundCorners(popup, true)

    // Спросить у страницы её размер и подогнать окно.
    function measure(view) {
        view.executeJavaScript(
            "console.log('CRUMB_SIZE:' + JSON.stringify([" +
            "Math.max(document.documentElement.scrollWidth, document.body ? document.body.scrollWidth : 0)," +
            "Math.max(document.documentElement.scrollHeight, document.body ? document.body.scrollHeight : 0)]))")
    }

    Loader {
        id: loader
        anchors.fill: parent
        anchors.margins: 1
        active: false
        sourceComponent: BrowserView {
            id: pv
            initialUrl: popup.url
            onLoadingStateChanged: if (!loading) { popup.measure(pv); remeasure.restart() }
            onConsoleMessage: (m) => {
                if (!m.startsWith("CRUMB_SIZE:")) return
                const s = JSON.parse(m.substring(11))
                const right = popup.x + popup.width
                popup.width = Math.max(200, Math.min(800, s[0] + 2))
                popup.height = Math.max(80, Math.min(600, s[1] + 2))
                popup.x = Math.max(8, right - popup.width)
            }
            // Окна, которые расширение открывает, станут вкладками (см. strayUrlRequested).
            onPageClosed: popup.close()
            Timer { id: remeasure; interval: 400; onTriggered: popup.measure(pv) }
        }
    }
}
