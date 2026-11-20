# Установщик Crumb (по образцу установщика Notepad You).
#   crumb_setup       — интерфейс установщика/удалятора (Qt QML), лежит рядом с Crumb.exe
#   crumb_setup_stub  — маленький стартовый exe без Qt (статический CRT)
#   crumb_pack        — упаковщик: stub + сжатая папка сборки = Crumb-Setup.exe
# Сборка установщика:  cmake --build --preset release --target installer

if(NOT WIN32)
  return()
endif()

# ------------------------------------------------------------ crumb_setup
qt_add_executable(crumb_setup WIN32
  setup/main.cpp
  setup/SetupBackend.h setup/SetupBackend.cpp
  common/Payload.h
  ${CMAKE_SOURCE_DIR}/resources/crumb.manifest
  ${CMAKE_SOURCE_DIR}/resources/crumb.rc
)
qt_add_qml_module(crumb_setup
  URI CrumbSetup
  VERSION 1.0
  QML_FILES
    setup/qml/SetupWindow.qml
    setup/qml/Hero.qml
)
target_include_directories(crumb_setup PRIVATE setup)
target_compile_definitions(crumb_setup PRIVATE CRUMB_VERSION="${PROJECT_VERSION}" NOMINMAX UNICODE _UNICODE)
target_link_libraries(crumb_setup PRIVATE Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2
  crumb_ui crumb_uiplugin Cabinet ole32 shell32 advapi32)
set_target_properties(crumb_setup PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CRUMB_OUT_DIR}")
target_compile_options(crumb_setup PRIVATE /W3 /utf-8 /permissive-)

get_target_property(_qmake Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_qtbin "${_qmake}" DIRECTORY)
add_custom_command(TARGET crumb_setup POST_BUILD
  COMMAND "${_qtbin}/windeployqt.exe" --release --no-translations --no-system-d3d-compiler
          --no-opengl-sw --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/setup/qml"
          --qmldir "${CMAKE_SOURCE_DIR}/ui/qml" "$<TARGET_FILE:crumb_setup>"
  COMMAND ${CMAKE_COMMAND} -DOUT_DIR=${CRUMB_OUT_DIR} -P "${CMAKE_SOURCE_DIR}/cmake/TrimQtDeploy.cmake"
  COMMENT "windeployqt: Qt для установщика"
  VERBATIM)

# ------------------------------------------------------- stub и упаковщик
add_executable(crumb_setup_stub WIN32 stub/stub_main.cpp common/Payload.h stub/stub.manifest stub/stub.rc)
add_executable(crumb_pack pack/pack_main.cpp common/Payload.h)
foreach(t crumb_setup_stub crumb_pack)
  # Статический CRT: stub должен запускаться на чистой Windows без runtime Visual C++.
  set_property(TARGET ${t} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
  target_compile_definitions(${t} PRIVATE NOMINMAX UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
  target_compile_options(${t} PRIVATE /W3 /utf-8 /permissive-)
  target_link_libraries(${t} PRIVATE Cabinet shell32)
endforeach()

# ------------------------------------------------------- готовый установщик
set(CRUMB_INSTALLER_DIR "${CMAKE_BINARY_DIR}/dist")
add_custom_target(installer
  COMMAND $<TARGET_FILE:crumb_pack> --input "${CRUMB_OUT_DIR}" --stub $<TARGET_FILE:crumb_setup_stub>
          --out "${CRUMB_INSTALLER_DIR}/Crumb-Setup.exe"
  COMMAND ${CMAKE_COMMAND} -E copy "${CRUMB_INSTALLER_DIR}/Crumb-Setup.exe" "${CRUMB_INSTALLER_DIR}/Crumb-Uninstall.exe"
  DEPENDS crumb crumb_helper crumb_setup crumb_setup_stub crumb_pack
  COMMENT "Упаковка Crumb-Setup.exe и Crumb-Uninstall.exe"
  VERBATIM)
