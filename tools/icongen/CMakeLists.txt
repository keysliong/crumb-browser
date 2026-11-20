# crumb_icongen — пересобрать иконку из SVG:
#   cmake --build --preset release --target icons
# Результат кладётся в resources/icons/build (crumb.ico и PNG), их подключают .rc-файлы.
# Вариант выбирается так:  cmake -DCRUMB_ICON_VARIANT=a build   (a…g)

find_package(Qt6 REQUIRED COMPONENTS Svg)
qt_add_executable(crumb_icongen main.cpp)
target_link_libraries(crumb_icongen PRIVATE Qt6::Gui Qt6::Svg)
set_target_properties(crumb_icongen PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CRUMB_OUT_DIR}")

set(CRUMB_ICON_VARIANT "f" CACHE STRING "Вариант иконки: a…g (f — глобус-печенька, металл)")
set(_icons "${CMAKE_SOURCE_DIR}/resources/icons")
add_custom_target(icons
  COMMAND $<TARGET_FILE:crumb_icongen>
          --big "${_icons}/crumb-${CRUMB_ICON_VARIANT}.svg"
          --small "${_icons}/crumb-${CRUMB_ICON_VARIANT}-small.svg"
          --out "${_icons}/build"
  DEPENDS crumb_icongen
  WORKING_DIRECTORY "${CRUMB_OUT_DIR}"
  COMMENT "Иконка Crumb, вариант ${CRUMB_ICON_VARIANT}"
  VERBATIM)
