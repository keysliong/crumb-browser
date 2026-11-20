// Поисковики Crumb: название, буква-запаска и официальный значок.
// Значки загружаются с сайтов самих поисковиков (без сторонних сервисов);
// пока сети нет — показывается буква.

pragma Singleton
import QtQuick

QtObject {
    readonly property var list: [
        { id: "google",     name: "Google",     letter: "G", icon: "https://www.google.com/favicon.ico" },
        { id: "yandex",     name: "Яндекс",     letter: "Я", icon: "https://yandex.ru/favicon.ico" },
        { id: "duckduckgo", name: "DuckDuckGo", letter: "D", icon: "https://duckduckgo.com/favicon.ico" },
        { id: "bing",       name: "Bing",       letter: "B", icon: "https://www.bing.com/favicon.ico" },
        { id: "brave",      name: "Brave",      letter: "B", icon: "https://brave.com/static-assets/images/brave-favicon.png" },
        { id: "ecosia",     name: "Ecosia",     letter: "E", icon: "https://www.ecosia.org/favicon.ico" },
        { id: "custom",     name: qsTr("Свой"), letter: "★", icon: "" }
    ]
    function find(id) { return list.find(e => e.id === id) || list[0] }
}
