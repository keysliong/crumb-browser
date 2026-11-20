// Всплывающее окно Crumb: раскрывается из-под кнопки с пружиной и гаснет при закрытии.
// Точка раскрытия — угол, к которому окно привязано (по умолчанию правый верхний,
// как у кнопок справа на панели). Скорость и «пружинистость» — из настроек анимаций.
import QtQuick
import QtQuick.Controls

Popup {
    id: pop
    popupType: Popup.Window
    transformOrigin: Popup.TopRight

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Prefs.dur(140); easing.type: Easing.OutQuad }
            NumberAnimation {
                property: "scale"; from: 0.86; to: 1; duration: Prefs.dur(340)
                easing.type: Easing.OutBack; easing.overshoot: 1.2 * Prefs.springStrength
            }
        }
    }
    exit: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Prefs.dur(120); easing.type: Easing.InQuad }
            NumberAnimation { property: "scale"; from: 1; to: 0.94; duration: Prefs.dur(120); easing.type: Easing.InQuad }
        }
    }
}
