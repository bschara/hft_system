add_executable(order_book_tests order_book_test.cpp)

target_link_libraries(order_book_tests
    PRIVATE
        OrderBook
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(order_book_tests)
