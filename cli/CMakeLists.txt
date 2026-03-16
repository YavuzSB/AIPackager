add_executable(aipackager-cli
    src/main.cpp
)

target_link_libraries(aipackager-cli
    PRIVATE
        aipackager_core
)

target_compile_features(aipackager-cli PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(aipackager-cli PRIVATE /W4 /permissive-)
else()
    target_compile_options(aipackager-cli PRIVATE -Wall -Wextra -Wpedantic)
endif()
