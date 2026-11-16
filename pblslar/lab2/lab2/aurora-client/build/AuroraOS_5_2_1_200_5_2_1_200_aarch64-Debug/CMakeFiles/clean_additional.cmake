# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles/ru.mai.music_collection_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/ru.mai.music_collection_autogen.dir/ParseCache.txt"
  "ru.mai.music_collection_autogen"
  )
endif()
