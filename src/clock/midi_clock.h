#pragma once
#include <cstdint>
#include <atomic>
#include <thread>
#include <mutex>

enum class ClockMode : uint8_t { OFF, MASTER, SLAVE };

class MidiClock {
public:
    MidiClock() = default;
    ~MidiClock();

    void init(std::atomic<int>* conn_fd, std::mutex* serial_mutex);

    void set_mode(ClockMode mode);
    ClockMode mode() const;

    void set_bpm(int bpm);
    int bpm() const;
    int estimated_bpm() const;

    void start_transport();
    void stop_transport();
    bool is_running() const;

    void on_tick();
    int tick_count() const;

    static uint32_t tick_interval_us(int bpm);

private:
    void clock_thread_func();

    std::atomic<ClockMode> m_mode{ClockMode::OFF};
    std::atomic<int> m_bpm{120};
    std::atomic<bool> m_transport_running{false};
    std::atomic<int> m_tick_count{0};

    std::atomic<int> m_estimated_bpm{0};
    uint32_t m_last_tick_us{0};
    int m_tick_samples{0};
    uint32_t m_tick_intervals_us{0};

    std::thread m_clock_thread;

    std::atomic<int>* m_conn_fd{nullptr};
    std::mutex* m_serial_mutex{nullptr};
};
