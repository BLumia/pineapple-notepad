find_package(Qt6 6.8 REQUIRED COMPONENTS Test)

add_executable(kshlexer_fold_test)
target_sources(kshlexer_fold_test PRIVATE
    kshlexer_fold_test.cpp
    mockdocument.h
)
target_link_libraries(kshlexer_fold_test PRIVATE
    pineapple-notepad-lib
    Qt6::Test
)
add_test(NAME kshlexer_fold_test COMMAND kshlexer_fold_test)
# Qt Test on Windows writes via QtLogger; force it to stderr so ctest --output-on-failure works.
set_tests_properties(kshlexer_fold_test PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
)

add_executable(kshlexer_format_test)
target_sources(kshlexer_format_test PRIVATE
    kshlexer_format_test.cpp
    mockdocument.h
)
target_link_libraries(kshlexer_format_test PRIVATE
    pineapple-notepad-lib
    Qt6::Test
)
add_test(NAME kshlexer_format_test COMMAND kshlexer_format_test)
set_tests_properties(kshlexer_format_test PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
)

add_executable(kshlexer_indentfold_test)
target_sources(kshlexer_indentfold_test PRIVATE
    kshlexer_indentfold_test.cpp
    mockdocument.h
)
target_link_libraries(kshlexer_indentfold_test PRIVATE
    pineapple-notepad-lib
    Qt6::Test
)
add_test(NAME kshlexer_indentfold_test COMMAND kshlexer_indentfold_test)
set_tests_properties(kshlexer_indentfold_test PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
)

add_executable(sciedit_ksh_test)
target_sources(sciedit_ksh_test PRIVATE
    sciedit_ksh_test.cpp
)
target_link_libraries(sciedit_ksh_test PRIVATE
    pineapple-notepad-lib
    Qt6::Test
    Qt6::Widgets
)
add_test(NAME sciedit_ksh_test COMMAND sciedit_ksh_test)
set_tests_properties(sciedit_ksh_test PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
)

add_executable(comment_test)
target_sources(comment_test PRIVATE
    comment_test.cpp
)
target_link_libraries(comment_test PRIVATE
    pineapple-notepad-lib
    Qt6::Test
    Qt6::Widgets
)
add_test(NAME comment_test COMMAND comment_test)
set_tests_properties(comment_test PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
)

add_executable(case_convert_test)
target_sources(case_convert_test PRIVATE
    case_convert_test.cpp
)
target_link_libraries(case_convert_test PRIVATE
    pineapple-notepad-lib
    Qt6::Test
    Qt6::Widgets
)
add_test(NAME case_convert_test COMMAND case_convert_test)
set_tests_properties(case_convert_test PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
)

add_executable(tab_space_convert_test)
target_sources(tab_space_convert_test PRIVATE
    tab_space_convert_test.cpp
)
target_link_libraries(tab_space_convert_test PRIVATE
    pineapple-notepad-lib
    Qt6::Test
    Qt6::Widgets
)
add_test(NAME tab_space_convert_test COMMAND tab_space_convert_test)
set_tests_properties(tab_space_convert_test PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
)

add_executable(encode_decode_test)
target_sources(encode_decode_test PRIVATE
    encode_decode_test.cpp
)
target_link_libraries(encode_decode_test PRIVATE
    pineapple-notepad-lib
    Qt6::Test
    Qt6::Widgets
)
add_test(NAME encode_decode_test COMMAND encode_decode_test)
set_tests_properties(encode_decode_test PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
)

add_executable(license_verifier_test)
target_sources(license_verifier_test PRIVATE
    license_verifier_test.cpp
)
target_link_libraries(license_verifier_test PRIVATE
    pineapple-notepad-lib
    Qt6::Test
)
add_test(NAME license_verifier_test COMMAND license_verifier_test)
set_tests_properties(license_verifier_test PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
)

add_executable(languageconfigstore_test)
target_sources(languageconfigstore_test PRIVATE
    languageconfigstore_test.cpp
)
target_link_libraries(languageconfigstore_test PRIVATE
    pineapple-notepad-lib
    Qt6::Test
)
add_test(NAME languageconfigstore_test COMMAND languageconfigstore_test)
set_tests_properties(languageconfigstore_test PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
)

add_executable(exporters_test)
target_sources(exporters_test PRIVATE
    exporters_test.cpp
)
target_link_libraries(exporters_test PRIVATE
    pineapple-notepad-lib
    Qt6::Test
    Qt6::Widgets
)
add_test(NAME exporters_test COMMAND exporters_test)
set_tests_properties(exporters_test PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
)

add_executable(mark_test)
target_sources(mark_test PRIVATE
    mark_test.cpp
)
target_link_libraries(mark_test PRIVATE
    pineapple-notepad-lib
    Qt6::Test
    Qt6::Widgets
)
add_test(NAME mark_test COMMAND mark_test)
set_tests_properties(mark_test PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
)

add_executable(spellcheck_style_test)
target_sources(spellcheck_style_test PRIVATE
    spellcheck_style_test.cpp
)
target_link_libraries(spellcheck_style_test PRIVATE
    pineapple-notepad-lib
    Qt6::Test
    Qt6::Widgets
)
add_test(NAME spellcheck_style_test COMMAND spellcheck_style_test)
set_tests_properties(spellcheck_style_test PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
)

add_executable(searchresultlexer_test)
target_sources(searchresultlexer_test PRIVATE
    searchresultlexer_test.cpp
    mockdocument.h
)
target_link_libraries(searchresultlexer_test PRIVATE
    pineapple-notepad-lib
    Qt6::Test
)
add_test(NAME searchresultlexer_test COMMAND searchresultlexer_test)
set_tests_properties(searchresultlexer_test PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
)
