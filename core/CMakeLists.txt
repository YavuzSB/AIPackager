add_library(aipackager_core STATIC
    src/Scanner.cpp
    src/ChunkManager.cpp
    src/IndexBuilder.cpp
    src/Packager.cpp
)

target_include_directories(aipackager_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(aipackager_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(aipackager_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(aipackager_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
