cmake_minimum_required(VERSION 3.16)
project(shadpkg)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# ╔══════════════════════════════════════════════════════════════════════════╗
# ║  C++23 required for std::byteswap and modern features                   ║
# ╚══════════════════════════════════════════════════════════════════════════╝
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ╔══════════════════════════════════════════════════════════════════════════╗
# ║  Options                                                                  ║
# ╚══════════════════════════════════════════════════════════════════════════╝
option(BUILD_GUI "Build the GUI application" ON)

# ╔══════════════════════════════════════════════════════════════════════════╗
# ║  Find Required Packages                                                   ║
# ╚══════════════════════════════════════════════════════════════════════════╝
find_package(ZLIB REQUIRED)
find_package(CryptoPP REQUIRED)
find_package(fmt REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(capstone REQUIRED)

if(BUILD_GUI)
    find_package(imgui REQUIRED)
    find_package(glfw3 REQUIRED)
    find_package(OpenGL REQUIRED)
endif()

# ╔══════════════════════════════════════════════════════════════════════════╗
# ║  Core Library (shared between CLI and GUI)                               ║
# ╚══════════════════════════════════════════════════════════════════════════╝
set(CORE_SOURCES
    core/file_format/pkg.cpp
    core/file_format/trp.cpp
    core/file_format/psf.cpp
    core/file_format/rif_generator.cpp
    core/file_format/pkg_type.cpp
    core/file_format/iso_extractor.cpp
    core/patcher/psn_bypass.cpp
    core/patcher/elf_reconstruct.cpp
    core/crypto/crypto.cpp
    core/decompiler/DecompilerContext.cpp
    core/decompiler/analysis/DominatorAnalysis.cpp
    core/decompiler/analysis/StructuralAnalysis.cpp
    core/decompiler/analysis/SymbolAnalysis.cpp
    core/decompiler/analysis/SymbolDatabase.cpp
    core/decompiler/analysis/DataFlowAnalysis.cpp
    core/decompiler/analysis/JumpTableAnalysis.cpp
    core/decompiler/lifter/VariableAnalysis.cpp
    core/decompiler/codegen/CppEmitter.cpp
    core/decompiler/ir/AST.cpp
    common/io_file.cpp
    common/path_util.cpp
    common/error.cpp
    common/assert.cpp
    common/string_util.cpp
    common/ntapi_stub.cpp
    common/config.cpp
    common/logging/backend.cpp
    common/logging/filter.cpp
    common/logging/text_formatter.cpp
    common/thread.cpp
)

add_library(shadpkg_core STATIC ${CORE_SOURCES})

target_include_directories(shadpkg_core PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/core/file_format
    ${CMAKE_SOURCE_DIR}/core/crypto
    ${CMAKE_SOURCE_DIR}/common
)

target_link_libraries(shadpkg_core PUBLIC ZLIB::ZLIB fmt::fmt cryptopp::cryptopp capstone::capstone nlohmann_json::nlohmann_json)

if(WIN32)
    target_compile_definitions(shadpkg_core PRIVATE _WIN32_WINNT=NTDDI_WIN10_RS5)
    target_link_libraries(shadpkg_core PUBLIC onecore)
elseif(APPLE)
    target_link_libraries(shadpkg_core PUBLIC "-framework CoreFoundation")
endif()

# ╔══════════════════════════════════════════════════════════════════════════╗
# ║  CLI Executable                                                           ║
# ╚══════════════════════════════════════════════════════════════════════════╝
add_executable(shadpkg main.cpp)
target_link_libraries(shadpkg PRIVATE shadpkg_core)

if(WIN32)
    target_compile_definitions(shadpkg PRIVATE _WIN32_WINNT=NTDDI_WIN10_RS5)
endif()

# ╔══════════════════════════════════════════════════════════════════════════╗
# ║  GUI Executable                                                           ║
# ╚══════════════════════════════════════════════════════════════════════════╝
if(BUILD_GUI)
    set(GUI_SOURCES
        gui/main_gui.cpp
        gui/GUIContext.cpp
        gui/StyleManager.cpp
        gui/NavBar.cpp
        gui/ConsoleLog.cpp
        gui/views/ExtractorView.cpp
        gui/views/InspectorView.cpp
        gui/views/RIFView.cpp
        gui/views/SettingsView.cpp
        gui/views/DecompilerView.cpp
        gui/views/StructEditorView.cpp
    )

    add_executable(shadpkg-gui ${GUI_SOURCES})

    target_include_directories(shadpkg-gui PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/gui
        ${CMAKE_SOURCE_DIR}/gui/include
    )

    target_precompile_headers(shadpkg-gui PRIVATE <cassert> "common/assert.h")
    target_compile_definitions(shadpkg-gui PRIVATE IM_ASSERT=ASSERT)

    target_link_libraries(shadpkg-gui PRIVATE
        shadpkg_core
        imgui::imgui
        glfw
        OpenGL::GL
    )

    set(IMGUI_BINDINGS_DIR "")
    if(DEFINED imgui_PACKAGE_FOLDER_RELEASE)
        set(IMGUI_BINDINGS_DIR "${imgui_PACKAGE_FOLDER_RELEASE}/res/bindings")
    elseif(DEFINED imgui_PACKAGE_FOLDER_DEBUG)
        set(IMGUI_BINDINGS_DIR "${imgui_PACKAGE_FOLDER_DEBUG}/res/bindings")
    endif()

    if(IMGUI_BINDINGS_DIR)
        target_include_directories(shadpkg-gui PRIVATE "${IMGUI_BINDINGS_DIR}")
        target_sources(shadpkg-gui PRIVATE
            "${IMGUI_BINDINGS_DIR}/imgui_impl_glfw.cpp"
            "${IMGUI_BINDINGS_DIR}/imgui_impl_opengl3.cpp"
        )
    endif()

    if(WIN32)
        target_compile_definitions(shadpkg-gui PRIVATE _WIN32_WINNT=NTDDI_WIN10_RS5)
        # Windows-specific: Link shell32 for folder dialogs
        target_link_libraries(shadpkg-gui PRIVATE shell32 ole32)
        # Hide console window in release builds
        set_target_properties(shadpkg-gui PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
    elseif(APPLE)
        target_link_libraries(shadpkg-gui PRIVATE "-framework Cocoa" "-framework IOKit")
    endif()

    # ═══════════════════════════════════════════════════════════════════════
    #  Copy resources to build directory
    # ═══════════════════════════════════════════════════════════════════════
    if(EXISTS "${CMAKE_SOURCE_DIR}/gui/resources")
        add_custom_command(TARGET shadpkg-gui POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/gui/resources"
            "$<TARGET_FILE_DIR:shadpkg-gui>/resources"
        )
    endif()
endif()
