// Градиентные фоны домашней страницы. «theme» строится из цветов текущей темы,
// остальные — готовые наборы: первый цвет — основа, остальные — плавающие пятна.

pragma Singleton
import QtQuick

QtObject {
    readonly property var list: [
        { id: "theme",    name: qsTr("Цвета темы"), colors: [] },
        { id: "dawn",     name: qsTr("Рассвет"),    colors: ["#FFD3B5", "#FF8FAB", "#B39DDB", "#FFB88C"] },
        { id: "ocean",    name: qsTr("Океан"),      colors: ["#0B2447", "#1E88E5", "#26C6DA", "#3949AB"] },
        { id: "aurora",   name: qsTr("Аврора"),     colors: ["#0B1D3A", "#1DE9B6", "#7C4DFF", "#00B8D4"] },
        { id: "sunset",   name: qsTr("Закат"),      colors: ["#FF7E5F", "#FEB47B", "#C2185B", "#6A1B9A"] },
        { id: "forest",   name: qsTr("Лес"),        colors: ["#0F3D2E", "#71B280", "#2E7D32", "#A5D6A7"] },
        { id: "cosmos",   name: qsTr("Космос"),     colors: ["#0F0C29", "#302B63", "#7B1FA2", "#1A237E"] },
        { id: "candy",    name: qsTr("Карамель"),   colors: ["#F6D365", "#FDA085", "#F78CA0", "#FFCC80"] },
        { id: "mint",     name: qsTr("Мята"),       colors: ["#D9F7F1", "#A8EDEA", "#FED6E3", "#80CBC4"] },
        { id: "sakura",   name: qsTr("Сакура"),     colors: ["#FDE2F3", "#FBC2EB", "#A6C1EE", "#F48FB1"] },
        { id: "lavender", name: qsTr("Лаванда"),    colors: ["#EDE7F6", "#E0C3FC", "#8EC5FC", "#9575CD"] },
        { id: "midnight", name: qsTr("Полночь"),    colors: ["#141E30", "#243B55", "#3A1C71", "#0D47A1"] },
        { id: "lime",     name: qsTr("Лайм"),       colors: ["#F1F8E9", "#D4FC79", "#96E6A1", "#43A047"] },
        { id: "fire",     name: qsTr("Пламя"),      colors: ["#3E0A0A", "#F12711", "#F5AF19", "#FF5722"] },
        { id: "peach",    name: qsTr("Персик"),     colors: ["#FFF1E6", "#FFDAB9", "#FFB6A3", "#FFCC99"] }
    ]
    function find(id) { return list.find(g => g.id === id) || list[0] }
}
