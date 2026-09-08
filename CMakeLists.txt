cmake_minimum_required(VERSION 3.21)
project(SkysFalloutCheats VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "Fallout New Vegas requires a 32-bit (Win32) plugin. Configure with -A Win32")
endif()

add_library(SkysFalloutCheats SHARED
  src/main.cpp
  src/core/Log.cpp
  src/core/Config.cpp
  src/core/Compat.cpp
  src/core/FeatureRegistry.cpp
  src/core/ConsoleBridge.cpp
  src/core/App.cpp
  src/core/Input.cpp
  src/core/Hotkeys.cpp
  src/core/SearchIndex.cpp
  src/core/GameState.cpp
  src/core/UiHud.cpp
  src/core/Cheats.cpp
  src/core/Gameplay.cpp
  src/core/CompanionFollow.cpp
  src/core/Boot.cpp
  src/core/Diag.cpp
  src/core/Isolation.cpp
  src/core/GameWorkQueue.cpp
  src/core/NvseMessages.cpp
  src/render/D3D9Hook.cpp
  src/render/ImGuiBackend.cpp
  src/render/WorldToScreen.cpp
  src/ui/Theme.cpp
  src/ui/Notifications.cpp
  src/ui/MainMenu.cpp
  src/ui/Hud.cpp
  src/ui/SearchOverlay.cpp
  src/features/PlayerFeature.cpp
  src/features/WeaponsFeature.cpp
  src/features/InventoryFeature.cpp
  src/features/SkillsFeature.cpp
  src/features/NpcFeature.cpp
  src/features/WorldFeature.cpp
  src/features/TeleportFeature.cpp
  src/features/QuestFeature.cpp
  src/features/CameraFeature.cpp
  src/features/EspFeature.cpp
  src/features/PresetsFeature.cpp
  src/features/SettingsFeature.cpp
  src/features/UtilityFeature.cpp
  src/features/DebugFeature.cpp
  src/features/RegisterFeatures.cpp
  external/imgui/imgui.cpp
  external/imgui/imgui_draw.cpp
  external/imgui/imgui_tables.cpp
  external/imgui/imgui_widgets.cpp
  external/imgui/backends/imgui_impl_dx9.cpp
  external/imgui/backends/imgui_impl_win32.cpp
  external/minhook/src/buffer.c
  external/minhook/src/hook.c
  external/minhook/src/trampoline.c
  external/minhook/src/hde/hde32.c
)

target_include_directories(SkysFalloutCheats PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/external
  ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
  ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends
  ${CMAKE_CURRENT_SOURCE_DIR}/external/minhook/include
  ${CMAKE_CURRENT_SOURCE_DIR}/external/xnvse
)

target_compile_definitions(SkysFalloutCheats PRIVATE
  WIN32_LEAN_AND_MEAN
  NOMINMAX
  _CRT_SECURE_NO_WARNINGS
  IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
)

target_link_libraries(SkysFalloutCheats PRIVATE d3d9)

set_target_properties(SkysFalloutCheats PROPERTIES
  OUTPUT_NAME "SkysFalloutCheats"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  PREFIX ""
)

if(DEFINED ENV{FalloutNVPath})
  set(FNV_PLUGINS "$ENV{FalloutNVPath}/Data/NVSE/Plugins")
  add_custom_command(TARGET SkysFalloutCheats POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${FNV_PLUGINS}/SkysFalloutCheats"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:SkysFalloutCheats>" "${FNV_PLUGINS}/SkysFalloutCheats.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_SOURCE_DIR}/data_package" "${FNV_PLUGINS}/SkysFalloutCheats"
    COMMENT "Deploying Sky's Fallout Cheats to FalloutNVPath"
  )
endif()
