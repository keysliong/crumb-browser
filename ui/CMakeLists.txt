# Crumb.Ui — общий QML-модуль браузера и установщика:
# Material You (Theme), CookieButton, CookieSwitch и прочие компоненты.

qt_add_library(crumb_ui STATIC)

qt_add_qml_module(crumb_ui
  URI Crumb.Ui
  VERSION 1.0
  OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qmlmodules/Crumb/Ui"
  SOURCES
    src/MaterialColor.h src/MaterialColor.cpp
    src/Theme.h src/Theme.cpp
    src/Prefs.h src/Prefs.cpp
  QML_FILES
    qml/CookieShape.qml
    qml/CookieButton.qml
    qml/CookieSwitch.qml
    qml/ThemedMenu.qml
    qml/ThemedMenuItem.qml
    qml/ThemedMenuSeparator.qml
    qml/CText.qml
    qml/CookieSlider.qml
    qml/CookiePopup.qml
)

qt_add_resources(crumb_ui "crumb_ui_fonts"
  PREFIX "/crumb"
  FILES fonts/RobotoFlex.ttf
)

# Иконка Crumb (генерирует цель icons) — для домашней страницы, настроек, установщика.
set(_icon_png "${CMAKE_SOURCE_DIR}/resources/icons/build/crumb-256.png")
set_source_files_properties("${_icon_png}" PROPERTIES QT_RESOURCE_ALIAS "icon.png")
qt_add_resources(crumb_ui "crumb_ui_icon"
  PREFIX "/crumb"
  FILES "${_icon_png}"
)

target_include_directories(crumb_ui PUBLIC src)
target_link_libraries(crumb_ui PUBLIC Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2)
if(WIN32)
  target_link_libraries(crumb_ui PUBLIC dwmapi winmm)
endif()
if(MSVC)
  target_compile_options(crumb_ui PRIVATE /W3 /utf-8 /permissive-)
endif()
