# Third-party dependencies, fetched with FetchContent (the one mechanism this
# project uses; see ARCHITECTURE.md section 9).
include(FetchContent)

# tl::expected (CC0, header only). Its own CMakeLists pulls in extra tooling,
# so only the sources are downloaded and a small interface target is defined
# here. SOURCE_SUBDIR points at a folder without a CMakeLists.txt so
# FetchContent_MakeAvailable does not add the project.
FetchContent_Declare(tl_expected
    URL https://github.com/TartanLlama/expected/archive/refs/tags/v1.1.0.tar.gz
    URL_HASH SHA256=1db357f46dd2b24447156aaf970c4c40a793ef12a8a9c2ad9e096d9801368df6
    SOURCE_SUBDIR do-not-add
    DOWNLOAD_EXTRACT_TIMESTAMP ON)
FetchContent_MakeAvailable(tl_expected)

if(NOT TARGET tl::expected)
    add_library(tl_expected INTERFACE)
    add_library(tl::expected ALIAS tl_expected)
    target_include_directories(tl_expected SYSTEM INTERFACE "${tl_expected_SOURCE_DIR}/include")
endif()
