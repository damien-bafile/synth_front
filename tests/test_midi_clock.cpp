#include <gtest/gtest.h>
#include "clock/midi_clock.h"
#include <thread>

static std::atomic<int> dummy_fd{-1};
static std::mutex dummy_mutex;

struct MidiClockTest : ::testing::Test {
    MidiClock clock;
    void SetUp() override {
        clock.init(&dummy_fd, &dummy_mutex);
        clock.set_mode(ClockMode::OFF);
    }
};

TEST_F(MidiClockTest, DefaultState) {
    EXPECT_EQ(clock.mode(), ClockMode::OFF);
    EXPECT_EQ(clock.bpm(), 120);
    EXPECT_FALSE(clock.is_running());
    EXPECT_EQ(clock.tick_count(), 0);
}

TEST_F(MidiClockTest, SetMode) {
    clock.set_mode(ClockMode::MASTER);
    EXPECT_EQ(clock.mode(), ClockMode::MASTER);
    clock.set_mode(ClockMode::SLAVE);
    EXPECT_EQ(clock.mode(), ClockMode::SLAVE);
    clock.set_mode(ClockMode::OFF);
    EXPECT_EQ(clock.mode(), ClockMode::OFF);
}

TEST_F(MidiClockTest, SetBpm) {
    clock.set_bpm(140);
    EXPECT_EQ(clock.bpm(), 140);
}

TEST_F(MidiClockTest, BpmClampLow) {
    clock.set_bpm(5);
    EXPECT_EQ(clock.bpm(), 20);
}

TEST_F(MidiClockTest, BpmClampHigh) {
    clock.set_bpm(500);
    EXPECT_EQ(clock.bpm(), 300);
}

TEST_F(MidiClockTest, TransportStartStop) {
    clock.set_mode(ClockMode::OFF);
    clock.start_transport();
    EXPECT_TRUE(clock.is_running());
    clock.stop_transport();
    EXPECT_FALSE(clock.is_running());
}

TEST_F(MidiClockTest, OnTickIncrementsCountInSlaveMode) {
    clock.set_mode(ClockMode::SLAVE);
    clock.on_tick();
    EXPECT_EQ(clock.tick_count(), 1);
    clock.on_tick();
    EXPECT_EQ(clock.tick_count(), 2);
}

TEST_F(MidiClockTest, OnTickDoesNothingInOffMode) {
    clock.set_mode(ClockMode::OFF);
    clock.on_tick();
    EXPECT_EQ(clock.tick_count(), 0);
}

TEST_F(MidiClockTest, TickIntervalCalculation) {
    // 120 BPM: interval = 60000000 / (120 * 24) = 20833 us
    EXPECT_EQ(MidiClock::tick_interval_us(120), 20833U);
    // 60 BPM: interval = 60000000 / (60 * 24) = 41666 us
    EXPECT_EQ(MidiClock::tick_interval_us(60), 41666U);
}
