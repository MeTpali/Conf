# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles/ru.mai.temperature_converter_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/ru.mai.temperature_converter_autogen.dir/ParseCache.txt"
  "ru.mai.temperature_converter_autogen"
  )
endif()
