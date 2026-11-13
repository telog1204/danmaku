# Android 版のネイティブ部分（libgame.so）。ソースはリポジトリの src/common（両OS共通）と src/android（Android固有）を直接参照する
cmake_minimum_required(VERSION 3.22.1)
project(game LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

# DxLib Android版のライブラリとヘッダー（ABIごとのフォルダ）
if(NOT DXLIB_ANDROID_DIR)
    message(FATAL_ERROR "DXLIB_ANDROID_DIR（DxLib Android版を展開した DxLib_Android フォルダ）が指定されていません")
endif()
set(DXLIB_LIB_DIR "${DXLIB_ANDROID_DIR}/Lib_AndroidStudio/${ANDROID_ABI}")
if(NOT EXISTS "${DXLIB_LIB_DIR}/DxLib.h")
    message(FATAL_ERROR "DxLib Android版が見つかりません: ${DXLIB_LIB_DIR}")
endif()

file(GLOB COMMON_SOURCES CONFIGURE_DEPENDS "${REPO_ROOT}/src/common/*.cpp")
list(FILTER COMMON_SOURCES EXCLUDE REGEX "/pch\\.cpp$")  # Visual Studio のプリコンパイル済みヘッダー用なので使わない
file(GLOB ANDROID_SOURCES CONFIGURE_DEPENDS "${REPO_ROOT}/src/android/*.cpp")

# ライブラリの名前（game）は AndroidManifest.xml の android.app.lib_name と合わせる
add_library(game SHARED ${COMMON_SOURCES} ${ANDROID_SOURCES})
target_include_directories(game PRIVATE "${REPO_ROOT}/src/common" "${REPO_ROOT}/src/android" "${DXLIB_LIB_DIR}")
target_link_directories(game PRIVATE "${DXLIB_LIB_DIR}")

# リンクするライブラリは DxLib Android版に付属する Android Studio 用サンプルの CMakeLists.txt と同じ
target_link_libraries(game
    android log GLESv1_CM EGL GLESv2 OpenSLES m
    DxLib DxUseCLib jpeg png zlib tiff
    theora_static vorbis_static vorbisfile_static ogg_static bullet opus)
