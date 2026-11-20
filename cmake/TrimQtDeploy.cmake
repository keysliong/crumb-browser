# Убирает из папки сборки то, что windeployqt кладёт «на всякий случай», а Crumb
# не использует: стили Qt Quick Controls кроме Basic, отладочные плагины QML и т.п.
# Меньше файлов — меньше установщик и быстрее его запуск.
# Запуск: cmake -DOUT_DIR=<папка> -P TrimQtDeploy.cmake

if(NOT OUT_DIR)
  message(FATAL_ERROR "OUT_DIR не задан")
endif()

set(_dirs
  qmltooling
  qml/QtQuick/Controls/FluentWinUI3
  qml/QtQuick/Controls/Fusion
  qml/QtQuick/Controls/Imagine
  qml/QtQuick/Controls/Material
  qml/QtQuick/Controls/Universal
  qml/QtQuick/Controls/Windows
  qml/QtQuick/NativeStyle
  qml/QtQuick/Particles
  qml/QtQuick/LocalStorage
  qml/QtQuick/VectorImage
)
set(_files
  Qt6QuickControls2FluentWinUI3StyleImpl.dll
  Qt6QuickControls2Fusion.dll Qt6QuickControls2FusionStyleImpl.dll
  Qt6QuickControls2Imagine.dll Qt6QuickControls2ImagineStyleImpl.dll
  Qt6QuickControls2Material.dll Qt6QuickControls2MaterialStyleImpl.dll
  Qt6QuickControls2Universal.dll Qt6QuickControls2UniversalStyleImpl.dll
  Qt6QuickControls2WindowsStyleImpl.dll
  Qt6QuickParticles.dll Qt6QuickVectorImage.dll Qt6QuickVectorImageGenerator.dll
  # из драйверов баз данных нужен только SQLite
  sqldrivers/qsqlibase.dll sqldrivers/qsqlmimer.dll sqldrivers/qsqloci.dll
  sqldrivers/qsqlodbc.dll sqldrivers/qsqlpsql.dll
)
foreach(d ${_dirs})
  file(REMOVE_RECURSE "${OUT_DIR}/${d}")
endforeach()
foreach(f ${_files})
  file(REMOVE "${OUT_DIR}/${f}")
endforeach()
