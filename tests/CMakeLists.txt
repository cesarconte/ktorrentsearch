add_executable(test_torrentsearch
    test_torrentsearch.cpp
)

target_link_libraries(test_torrentsearch PRIVATE
    ktorrentsearch_core
    Qt6::Test
)

add_test(NAME test_torrentsearch COMMAND test_torrentsearch)
