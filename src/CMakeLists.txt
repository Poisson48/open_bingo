find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)
pkg_check_modules(SECP256K1 REQUIRED IMPORTED_TARGET libsecp256k1)

# Warnings stricts — notre code uniquement (pas ZXing / FetchContent).
set(BINGO_WARN_FLAGS -Wall -Wextra -Wpedantic -Wno-unused-parameter)

add_library(qrcodegen STATIC
    ${CMAKE_SOURCE_DIR}/third_party/qrcodegen/qrcodegen.cpp
)
target_include_directories(qrcodegen PUBLIC ${CMAKE_SOURCE_DIR}/third_party/qrcodegen)

add_library(net STATIC
    net/nostr.cpp
    net/relayclient.cpp
    net/relaypool.cpp
    net/pushclient.cpp
    net/opensubtitles_com.cpp
    net/opensubtitles_org.cpp
    net/opensubtitlesclient.cpp
)
target_include_directories(net PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(net PUBLIC Qt6::Core Qt6::Network Qt6::WebSockets)
target_compile_options(net PRIVATE ${BINGO_WARN_FLAGS})

add_library(cryptolayer STATIC net/crypto.cpp)
target_include_directories(cryptolayer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cryptolayer PUBLIC Qt6::Core net PkgConfig::SODIUM PkgConfig::SECP256K1)
target_compile_options(cryptolayer PRIVATE ${BINGO_WARN_FLAGS})

add_library(core STATIC
    core/generator.cpp
    core/jsoncodec.cpp
    core/sharecodec.cpp
    core/pairing.cpp
    core/crdt.cpp
    core/payload.cpp
    core/csv.cpp
    core/projectcsv.cpp
    core/srtcodec.cpp
    core/phraseextract.cpp
    core/zip.cpp
)
target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(core PUBLIC ZLIB::ZLIB)
target_compile_options(core PRIVATE ${BINGO_WARN_FLAGS})

add_library(store STATIC store/database.cpp)
target_include_directories(store PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(store PUBLIC Qt6::Core Qt6::Sql core)
target_compile_options(store PRIVATE ${BINGO_WARN_FLAGS})

add_library(qrdecode STATIC app/qrdecode.cpp)
target_include_directories(qrdecode PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/app)
target_link_libraries(qrdecode PUBLIC Qt6::Gui ZXing::ZXing)
target_compile_options(qrdecode PRIVATE ${BINGO_WARN_FLAGS})

set(APPLIB_SOURCES
    app/appcontroller.cpp
    app/playlogic.cpp
    app/exportprint.cpp
    app/projectmodel.cpp
    app/projectsync.cpp
    app/platform.cpp
    app/permissions.cpp
    app/qrimageprovider.cpp
    app/theme.cpp
    app/updater.cpp
    app/mcpserver.cpp
    app/mcp_json.cpp
    app/mcp_tools.cpp
    app/mcp_tools_film.cpp
    app/mcp_tools_project.cpp
    app/filmassistant.cpp
)
if(BINGO_HAS_CAMERA)
    list(APPEND APPLIB_SOURCES app/qrscanner.cpp)
endif()

add_library(applib STATIC ${APPLIB_SOURCES})
target_include_directories(applib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(applib PUBLIC
    core cryptolayer net store qrcodegen qrdecode
    Qt6::Core Qt6::Network Qt6::Quick Qt6::Sql Qt6::WebSockets Qt6::PrintSupport Qt6::Widgets
    PkgConfig::SODIUM PkgConfig::SECP256K1
)
target_compile_options(applib PRIVATE ${BINGO_WARN_FLAGS})
if(BINGO_HAS_CAMERA)
    target_link_libraries(applib PUBLIC Qt6::Multimedia)
    target_compile_definitions(applib PUBLIC BINGO_HAS_CAMERA)
endif()

set(BINGO_VERSION_NAME "2.0.28" CACHE STRING "android:versionName")
set(BINGO_VERSION_CODE "1" CACHE STRING "android:versionCode")
# User-Agent OpenSubtitles (net) + applib
target_compile_definitions(net PUBLIC BINGO_APP_VERSION="${BINGO_VERSION_NAME}")
target_compile_definitions(applib PUBLIC BINGO_APP_VERSION="${BINGO_VERSION_NAME}")

qt_add_executable(openbingo app/main.cpp)

set(BINGO_LOGO "${CMAKE_SOURCE_DIR}/packaging/openbingo.png")
set_source_files_properties(${BINGO_LOGO} PROPERTIES QT_RESOURCE_ALIAS "openbingo.png")
qt_add_resources(openbingo "app_icons"
    PREFIX "/icons"
    FILES ${BINGO_LOGO}
)

set(BINGO_QML_FILES
    qml/Main.qml
    qml/ProjectsPage.qml
    qml/EditorPage.qml
    qml/ConfigPage.qml
    qml/CasesPage.qml
    qml/GagesPage.qml
    qml/FilmAssistPage.qml
    qml/FilmSearchBar.qml
    qml/SubtitleResultDelegate.qml
    qml/CueList.qml
    qml/GridsPage.qml
    qml/PrintPage.qml
    qml/PlayPage.qml
    qml/ScoresPage.qml
    qml/ColoDialog.qml
    qml/ColoTextField.qml
    qml/scrollutils.js
    qml/ColoMenu.qml
    qml/ColoComboBox.qml
    qml/ColoSpinBox.qml
    qml/ColoCheckBox.qml
    qml/Icon.qml
    qml/IconButton.qml
    qml/BingoButton.qml
    qml/BingoTabBar.qml
    qml/BingoGrid.qml
    qml/CellPicker.qml
    qml/PlayFullscreen.qml
    qml/ChangelogDialog.qml
    qml/ShareSheet.qml
)
if(BINGO_HAS_CAMERA)
    list(APPEND BINGO_QML_FILES qml/ScanPage.qml)
endif()

qt_add_qml_module(openbingo
    URI OpenBingo
    VERSION 1.0
    QML_FILES ${BINGO_QML_FILES}
)

target_link_libraries(openbingo PRIVATE
    applib cryptolayer net store qrcodegen
    Qt6::Core Qt6::Quick Qt6::QuickControls2 Qt6::Sql Qt6::WebSockets Qt6::PrintSupport Qt6::Widgets
    PkgConfig::SODIUM PkgConfig::SECP256K1
)
target_compile_options(openbingo PRIVATE ${BINGO_WARN_FLAGS})

set_property(TARGET openbingo PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android")
set_property(TARGET openbingo PROPERTY QT_ANDROID_TARGET_SDK_VERSION 34)
set_property(TARGET openbingo PROPERTY QT_ANDROID_VERSION_NAME "${BINGO_VERSION_NAME}")
set_property(TARGET openbingo PROPERTY QT_ANDROID_VERSION_CODE "${BINGO_VERSION_CODE}")

if(ANDROID)
    include(FetchContent)
    FetchContent_Declare(android_openssl
        GIT_REPOSITORY https://github.com/KDAB/android_openssl.git
        GIT_TAG master
    )
    FetchContent_MakeAvailable(android_openssl)
    include(${android_openssl_SOURCE_DIR}/android_openssl.cmake)
    add_android_openssl_libraries(openbingo)
endif()
