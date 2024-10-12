# Parent project does not export its library target, so this CML implicitly
# depends on being added from it, i.e. the testing is done only from the build
# tree and is not feasible from an install location

project(cpp-order-bookTests LANGUAGES CXX)

# ---- Dependencies ----

find_package(Catch2 REQUIRED)
include(Catch)

# ---- Tests ----

add_executable(cpp-order-book_test source/cpp-order-book_test.cpp)
target_link_libraries(
    cpp-order-book_test PRIVATE
    cpp-order-book_lib
    Catch2::Catch2WithMain
)
target_compile_features(cpp-order-book_test PRIVATE cxx_std_20)

catch_discover_tests(cpp-order-book_test)

# ---- End-of-file commands ----

add_folders(Test)
