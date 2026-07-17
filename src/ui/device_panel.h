/// @file device_panel.h
/// @brief ImGui tabs for viewing and selecting audio devices and serial ports.

#pragma once
#include <atomic>

/// Render device tabs (Playback, Recording, Serial) within an existing ImGui
/// BeginTabBar / EndTabBar context.
void RenderDeviceTabs(
    std::atomic<bool>& connected,
    std::atomic<bool>& audio_active,
    std::atomic<int>& conn_fd);
