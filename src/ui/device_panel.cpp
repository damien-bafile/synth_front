/// @file device_panel.cpp
/// @brief ImGui tabs for playback devices, recording devices, and serial ports.
///        Allows switching audio output and serial connection.

#include "device_panel.h"
#include "clock/midi_clock.h"
#include "audio/audio.h"
#include "serial/serial_port.h"
#include "serial/usb_detect.h"
#include <SDL3/SDL.h>
#include <imgui.h>
#include <string>
#include <vector>
#include <cstdio>

static std::string s_selected_playback;
static std::string s_connected_port;

static void connect_serial(const std::string& port,
                           std::atomic<int>& conn_fd,
                           std::atomic<bool>& connected) {
  int old = conn_fd.exchange(-1, std::memory_order_release);
  if (old >= 0) {
    SDL_Delay(50);
    serial_close(old);
  }
  connected.store(false, std::memory_order_release);

  int new_fd = serial_open(port.c_str(), 2000000);
  if (new_fd >= 0) {
    conn_fd.store(new_fd, std::memory_order_release);
    connected.store(true, std::memory_order_release);
    s_connected_port = port;
    fprintf(stderr, "Connected to %s\n", port.c_str());
  } else {
    fprintf(stderr, "Failed to open %s\n", port.c_str());
  }
}

void RenderDeviceTabs(std::atomic<bool>& connected,
                       std::atomic<bool>& audio_active,
                       std::atomic<int>& conn_fd) {

  if (ImGui::BeginTabItem("Playback")) {
    int count = 0;
    SDL_AudioDeviceID* devs = SDL_GetAudioPlaybackDevices(&count);
    bool is_default_active = s_selected_playback.empty() && audio_active.load();

    if (ImGui::Selectable("Default", is_default_active)) {
      int result = audio_restart_with(nullptr);
      audio_active.store(result == 0, std::memory_order_release);
      if (result == 0)
        s_selected_playback.clear();
    }

    if (devs) {
      for (int i = 0; i < count; i++) {
        const char* name = SDL_GetAudioDeviceName(devs[i]);
        std::string name_str(name ? name : "Unknown");
        bool is_active = (name_str == s_selected_playback) && audio_active.load();

        if (ImGui::Selectable(name_str.c_str(), is_active)) {
          int result = audio_restart_with(name_str.c_str());
          audio_active.store(result == 0, std::memory_order_release);
          if (result == 0)
            s_selected_playback = name_str;
        }
      }
      SDL_free(devs);
    }

    if (count == 0 && !devs)
      ImGui::TextDisabled("No playback devices found");

    ImGui::EndTabItem();
  }

  if (ImGui::BeginTabItem("Recording")) {
    int count = 0;
    SDL_AudioDeviceID* devs = SDL_GetAudioRecordingDevices(&count);
    if (devs && count > 0) {
      for (int i = 0; i < count; i++) {
        const char* name = SDL_GetAudioDeviceName(devs[i]);
        std::string name_str(name ? name : "Unknown");
        bool is_teensy = name_str.find("Teensy") != std::string::npos;
        if (is_teensy)
          ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
        ImGui::BulletText("%s", name_str.c_str());
        if (is_teensy)
          ImGui::PopStyleColor();
      }
      SDL_free(devs);
    } else {
      ImGui::TextDisabled("No recording devices found");
    }
    ImGui::EndTabItem();
  }

  if (ImGui::BeginTabItem("Serial")) {
    auto ports = find_serial_ports();
    std::string teensy = find_teensy_port();
    if (ports.empty()) {
      ImGui::TextDisabled("No serial ports found");
    } else {
      for (const auto& p : ports) {
        bool is_teensy_port = (p == teensy);
        bool is_connected_port = connected.load() && (p == s_connected_port);

        std::string label = p;
        if (is_connected_port)
          label += " (connected)";
        if (is_teensy_port)
          label += " [Teensy]";

        if (is_teensy_port)
          ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));

        if (ImGui::Selectable(label.c_str(), is_connected_port)) {
          if (!is_connected_port)
            connect_serial(p, conn_fd, connected);
        }

        if (is_teensy_port)
          ImGui::PopStyleColor();
      }
    }
    ImGui::EndTabItem();
  }
}

void RenderClockTab(MidiClock* clock) {
    if (!clock) return;

    const char* modes[] = {"OFF", "MASTER", "SLAVE"};
    ClockMode current = clock->mode();
    int current_idx = static_cast<int>(current);
    if (current_idx < 0 || current_idx > 2) current_idx = 0;

    ImGui::Text("Clock Source");
    ImGui::SameLine();
    if (ImGui::Combo("##clocksrc", &current_idx, modes, IM_ARRAYSIZE(modes))) {
        clock->set_mode(static_cast<ClockMode>(current_idx));
    }

    if (clock->mode() == ClockMode::MASTER) {
        int bpm = clock->bpm();
        ImGui::Text("BPM");
        ImGui::SameLine();
        if (ImGui::SliderInt("##bpm", &bpm, 20, 300)) {
            clock->set_bpm(bpm);
        }

        if (ImGui::Button(clock->is_running() ? "Stop" : "Play")) {
            if (clock->is_running())
                clock->stop_transport();
            else
                clock->start_transport();
        }
    }

    if (clock->mode() == ClockMode::SLAVE) {
        int ebpm = clock->estimated_bpm();
        ImGui::Text("Estimated BPM: %d", ebpm > 0 ? ebpm : 0);
    }
}
