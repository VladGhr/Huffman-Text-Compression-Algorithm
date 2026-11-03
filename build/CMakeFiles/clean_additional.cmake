# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\compressor_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\compressor_autogen.dir\\ParseCache.txt"
  "compressor_autogen"
  )
endif()
