include(FetchContent)
# Pin both the upstream commit (0.17.0) and archive bytes. Only this dependency is static.
function(neden_add_qtkeychain)
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_WITH_QT5 OFF)
    set(BUILD_TEST_APPLICATION OFF)
    set(BUILD_QTQUICK_DEMO OFF)
    set(BUILD_TRANSLATIONS OFF)
    set(BUILD_TESTING OFF)
    FetchContent_Declare(qtkeychain
        URL https://github.com/frankosterfeld/qtkeychain/archive/875f77d9f61bd97fd84cca47ce3bc71186dfbd09.tar.gz
        URL_HASH SHA256=f367072fdf43551104cc718e43056d218d0c6d81c1b1758603fb3cd87144f8cc
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(qtkeychain)
    set(NEDEN_QTKEYCHAIN_LICENSE "${qtkeychain_SOURCE_DIR}/COPYING" PARENT_SCOPE)
endfunction()
neden_add_qtkeychain()
