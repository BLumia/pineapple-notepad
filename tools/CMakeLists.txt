# Generate fallback style data for old-style lexers (build-time)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(FALLBACK_STYLES_CPP "${CMAKE_CURRENT_BINARY_DIR}/fallback_styles.cpp")

add_custom_command(
    OUTPUT "${FALLBACK_STYLES_CPP}"
    COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_CURRENT_SOURCE_DIR}/gen_style_data.py"
        "${lexilla_SOURCE_DIR}/include/LexicalStyles.iface"
        "${lexilla_SOURCE_DIR}/include/SciLexer.h"
        "${FALLBACK_STYLES_CPP}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/gen_style_data.py"
            "${lexilla_SOURCE_DIR}/include/LexicalStyles.iface"
            "${lexilla_SOURCE_DIR}/include/SciLexer.h"
    COMMENT "Generating fallback style data from LexicalStyles.iface"
)

add_custom_target(generate_fallback_styles DEPENDS "${FALLBACK_STYLES_CPP}")

# Shared lexer query library (no Qt dependency)
add_library(lexer_query_lib STATIC)
target_sources(lexer_query_lib PRIVATE
    lexer_query.cpp
    lexer_query.h
)
target_link_libraries(lexer_query_lib PUBLIC Lexilla::Lexilla)
target_include_directories(lexer_query_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
add_dependencies(lexer_query_lib generate_fallback_styles)

# CLI version
add_executable(lexer_info)
target_sources(lexer_info PRIVATE lexer_info.cpp)
target_link_libraries(lexer_info PRIVATE lexer_query_lib)

# GUI version
add_executable(lexer_info_gui WIN32)
set_target_properties(lexer_info_gui PROPERTIES AUTOMOC ON)
target_sources(lexer_info_gui PRIVATE
    lexer_info_gui.cpp
    lexerinfowindow.cpp
    lexerinfowindow.h
)
target_link_libraries(lexer_info_gui PRIVATE
    lexer_query_lib
    Qt6::Widgets
)

install(TARGETS lexer_info lexer_info_gui ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
