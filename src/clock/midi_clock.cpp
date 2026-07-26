#include "clock/midi_clock.h"
#include "protocol/protocol.h"
#include <chrono>
#include <thread>

MidiClock::~MidiClock() {
    set_mode(ClockMode::OFF);
}

void MidiClock::init(std::atomic<int>* conn_fd, std::mutex* serial_mutex) {
    m_conn_fd = conn_fd;
    m_serial_mutex = serial_mutex;
}

void MidiClock::set_mode(ClockMode new_mode) {
    ClockMode old = m_mode.exchange(new_mode);
    if (old == ClockMode::MASTER && new_mode != ClockMode::MASTER) {
        stop_transport();
    }
    int fd = m_conn_fd ? m_conn_fd->load(std::memory_order_relaxed) : -1;
    if (fd >= 0 && m_serial_mutex) {
        uint8_t mode_byte = (new_mode == ClockMode::SLAVE) ? 1 : 0;
        std::lock_guard<std::mutex> lock(*m_serial_mutex);
        packet_send(fd, PacketType::CLOCK_MODE, &mode_byte, 1);
    }
}

ClockMode MidiClock::mode() const { return m_mode.load(); }

void MidiClock::set_bpm(int bpm) {
    if (bpm < 20) bpm = 20;
    if (bpm > 300) bpm = 300;
    m_bpm.store(bpm);
}

int MidiClock::bpm() const { return m_bpm.load(); }
int MidiClock::estimated_bpm() const { return m_estimated_bpm.load(); }

void MidiClock::start_transport() {
    m_tick_count.store(0);
    m_transport_running.store(true);

    int fd = m_conn_fd ? m_conn_fd->load(std::memory_order_relaxed) : -1;
    if (m_mode.load() == ClockMode::MASTER) {
        if (fd >= 0 && m_serial_mutex) {
            std::lock_guard<std::mutex> lock(*m_serial_mutex);
            packet_send_transport(fd, PacketType::MIDI_START);
        }
        if (m_clock_thread.joinable())
            m_clock_thread.join();
        m_clock_thread = std::thread(&MidiClock::clock_thread_func, this);
    } else if (m_mode.load() == ClockMode::SLAVE) {
        m_last_tick_us = 0;
        m_tick_samples = 0;
        m_tick_intervals_us = 0;
    }
}

void MidiClock::stop_transport() {
    m_transport_running.store(false);

    if (m_mode.load() == ClockMode::MASTER) {
        if (m_clock_thread.joinable())
            m_clock_thread.join();
        int fd = m_conn_fd ? m_conn_fd->load(std::memory_order_relaxed) : -1;
        if (fd >= 0 && m_serial_mutex) {
            std::lock_guard<std::mutex> lock(*m_serial_mutex);
            packet_send_transport(fd, PacketType::MIDI_STOP);
        }
    }
}

bool MidiClock::is_running() const { return m_transport_running.load(); }

void MidiClock::on_tick() {
    if (m_mode.load() != ClockMode::SLAVE)
        return;
    m_tick_count.fetch_add(1);

    auto now = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    if (m_last_tick_us > 0) {
        uint32_t interval = now - m_last_tick_us;
        if (interval < 200000) {
            m_tick_intervals_us += interval;
            m_tick_samples++;
            if (m_tick_samples >= 12) {
                uint32_t avg = m_tick_intervals_us / m_tick_samples;
                if (avg > 0) {
                    m_estimated_bpm.store(60000000UL / (avg * 24));
                }
                m_tick_samples = 0;
                m_tick_intervals_us = 0;
            }
        }
    }
    m_last_tick_us = now;
}

int MidiClock::tick_count() const { return m_tick_count.load(); }

void MidiClock::clock_thread_func() {
    while (m_transport_running.load() && m_mode.load() == ClockMode::MASTER) {
        int fd = m_conn_fd ? m_conn_fd->load(std::memory_order_relaxed) : -1;
        if (fd >= 0 && m_serial_mutex) {
            std::lock_guard<std::mutex> lock(*m_serial_mutex);
            packet_send(fd, PacketType::MIDI_CLOCK, nullptr, 0);
        }

        uint32_t interval_us = tick_interval_us(m_bpm.load());
        auto deadline = std::chrono::steady_clock::now()
                      + std::chrono::microseconds(interval_us);

        // Coarse sleep, leaving ~1ms for spin-wait
        if (interval_us > 1000) {
            std::this_thread::sleep_for(
                std::chrono::microseconds(interval_us - 1000));
        }

        // Spin-wait for tight jitter
        while (std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }
    }
}

uint32_t MidiClock::tick_interval_us(int bpm) {
    return 60000000 / (static_cast<uint32_t>(bpm) * 24);
}
