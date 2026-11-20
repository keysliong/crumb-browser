# Crumb — браузер на CEF (Chrome style) + Qt 6 QML.
# Сборка: см. README.md. Генератор — Visual Studio 2022 (Ninja не подходит:
# cmake-скрипты CEF при Ninja обнуляют глобальные CMAKE_CXX_FLAGS).

cmake_minimum_required(VERSION 3.24)
project(Crumb VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# В «minimal»-дистрибутиве CEF есть только Release-бинарники.
set(CMAKE_CONFIGURATION_TYPES "Release;RelWithDebInfo" CACHE STRING "" FORCE)

# ------------------------------------------------------------------------ CEF
# CEF с проприетарными кодеками (AAC/H.264 — Яндекс Музыка и т.п.), если он скачан
# (scripts/setup-codecs.ps1), иначе — обычная сборка с cef-builds.spotifycdn.com.
if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/cef-codecs/cmake/FindCEF.cmake")
  set(_crumb_cef_default "${CMAKE_SOURCE_DIR}/third_party/cef-codecs")
else()
  set(_crumb_cef_default "${CMAKE_SOURCE_DIR}/third_party/cef")
endif()
set(CEF_ROOT "${_crumb_cef_default}" CACHE PATH "Папка распакованного CEF binary distribution")
if(NOT EXISTS "${CEF_ROOT}/cmake/FindCEF.cmake")
  message(FATAL_ERROR "CEF не найден в ${CEF_ROOT}. Запустите scripts/setup-deps.ps1")
endif()

# Песочница CEF на Windows требует, чтобы приложение было DLL для bootstrap.exe —
# с Qt-приложением это несовместимо, поэтому выключаем.
set(USE_SANDBOX OFF CACHE BOOL "" FORCE)
# Qt собран с динамическим CRT (/MD) — обёртку CEF собираем так же.
set(CEF_RUNTIME_LIBRARY_FLAG "/MD" CACHE STRING "" FORCE)
set(USE_ATL OFF CACHE BOOL "" FORCE)

list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)
ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_RELEASE}" "${CEF_LIB_RELEASE}")

# Общие настройки для наших целей, которым нужен CEF. Флаги CEF (/W4 /WX,
# _HAS_EXCEPTIONS=0) сюда намеренно не тащим — с Qt они несовместимы.
function(crumb_use_cef target)
  target_include_directories(${target} PRIVATE "${CEF_ROOT}")
  target_link_libraries(${target} PRIVATE libcef_lib libcef_dll_wrapper ${CEF_STANDARD_LIBS})
  if(WIN32)
    target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    target_link_options(${target} PRIVATE /STACK:0x800000)
  endif()
endfunction()

set(CRUMB_OUT_DIR "${CMAKE_BINARY_DIR}/out/$<CONFIG>")

# ----------------------------------------------------- вспомогательный процесс
# Отдельный маленький exe без Qt для рендерера/GPU/сети — быстрее стартует.
add_executable(crumb_helper WIN32 src/helper/helper_main.cpp resources/crumb.manifest resources/crumb.rc)
crumb_use_cef(crumb_helper)
set_target_properties(crumb_helper PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CRUMB_OUT_DIR}")

# ------------------------------------------------------------------ сам Crumb
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Sql Network)
qt_standard_project_setup(REQUIRES 6.8)

# Общий QML-модуль Crumb.Ui (тема Material You, печеньки) — для браузера и установщика.
add_subdirectory(ui)

qt_add_executable(crumb WIN32
  src/main.cpp
  src/cef/CefTask.h
  src/cef/CrumbCefApp.h       src/cef/CrumbCefApp.cpp
  src/cef/BrowserClient.h     src/cef/BrowserClient.cpp
  src/cef/CefRuntime.h        src/cef/CefRuntime.cpp
  src/ui/BrowserView.h        src/ui/BrowserView.cpp
  src/ui/UrlUtil.h            src/ui/UrlUtil.cpp
  src/ui/Eyedropper.h         src/ui/Eyedropper.cpp
  src/storage/SettingsStore.h src/storage/SettingsStore.cpp
  src/home/Weather.h          src/home/Weather.cpp
  src/downloads/DownloadManager.h   src/downloads/DownloadManager.cpp
  src/extensions/ExtensionManager.h src/extensions/ExtensionManager.cpp
  src/system/DefaultBrowser.h       src/system/DefaultBrowser.cpp
  src/rewind/RewindStore.h          src/rewind/RewindStore.cpp
  src/rewind/RewindManager.h        src/rewind/RewindManager.cpp
  src/sync/SyncCrypto.h             src/sync/SyncCrypto.cpp
  src/sync/SyncManager.h            src/sync/SyncManager.cpp
  resources/crumb.manifest
  resources/crumb.rc
)

set_source_files_properties(qml/Engines.qml qml/Gradients.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(crumb
  URI Crumb
  VERSION 1.0
  QML_FILES
    qml/Main.qml
    qml/AddressBar.qml
    qml/TabItem.qml
    qml/ThemePopup.qml
    qml/HomePage.qml
    qml/SettingsPage.qml
    qml/EyedropperWindow.qml
    qml/Engines.qml
    qml/Gradients.qml
    qml/DownloadsPage.qml
    qml/ExtensionsPage.qml
    qml/ExtensionPopup.qml qml/RewindPopup.qml
)

# Client ID OAuth-приложения GitHub для входа в аккаунт (Device Flow). Публичный, не секрет.
set(CRUMB_GITHUB_CLIENT_ID "Ov23lif1IDrgv1cfJzAl" CACHE STRING "Client ID GitHub OAuth App для синхронизации")
target_compile_definitions(crumb PRIVATE CRUMB_GITHUB_CLIENT_ID="${CRUMB_GITHUB_CLIENT_ID}")
target_include_directories(crumb PRIVATE src src/cef src/ui src/storage src/home src/downloads src/extensions src/system src/rewind src/sync)
target_compile_definitions(crumb PRIVATE CRUMB_VERSION="${PROJECT_VERSION}")
target_link_libraries(crumb PRIVATE Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::Sql Qt6::Network crumb_ui crumb_uiplugin)
crumb_use_cef(crumb)
add_dependencies(crumb crumb_helper)
set_target_properties(crumb PROPERTIES
  OUTPUT_NAME "Crumb"
  RUNTIME_OUTPUT_DIRECTORY "${CRUMB_OUT_DIR}"
)
if(MSVC)
  target_compile_options(crumb PRIVATE /W3 /utf-8 /permissive-)
  target_compile_options(crumb_helper PRIVATE /W3 /utf-8 /permissive-)
endif()

# Кладём рядом с exe бинарники и ресурсы CEF.
COPY_FILES(crumb "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR_RELEASE}" "${CRUMB_OUT_DIR}")
COPY_FILES(crumb "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${CRUMB_OUT_DIR}")

# windeployqt: Qt-библиотеки и QML-модули рядом с exe, чтобы запускалось без PATH.
if(WIN32)
  get_target_property(_qmake Qt6::qmake IMPORTED_LOCATION)
  get_filename_component(_qtbin "${_qmake}" DIRECTORY)
  add_custom_command(TARGET crumb POST_BUILD
    COMMAND "${_qtbin}/windeployqt.exe" --release --no-translations --no-system-d3d-compiler
            --no-opengl-sw --qmldir "${CMAKE_SOURCE_DIR}/qml" --qmldir "${CMAKE_SOURCE_DIR}/ui/qml"
            "$<TARGET_FILE:crumb>"
    COMMAND ${CMAKE_COMMAND} -DOUT_DIR=${CRUMB_OUT_DIR} -P "${CMAKE_SOURCE_DIR}/cmake/TrimQtDeploy.cmake"
    COMMENT "windeployqt: копируем Qt рядом с Crumb.exe"
    VERBATIM)

  # Runtime Visual C++ (msvcp140, vcruntime140…) рядом с exe — чтобы Crumb
  # запускался на чистой Windows без отдельной установки VC++ Redistributable.
  set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
  set(CMAKE_INSTALL_UCRT_LIBRARIES FALSE)
  include(InstallRequiredSystemLibraries)
  foreach(_rt ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS})
    add_custom_command(TARGET crumb POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_rt}" "${CRUMB_OUT_DIR}"
      VERBATIM)
  endforeach()
endif()

# Генератор иконки (SVG → .ico/.png)
add_subdirectory(tools/icongen)

# Установщик и удалятор
add_subdirectory(installer)
