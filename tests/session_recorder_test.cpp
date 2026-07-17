#include "sdr/session_recorder.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace {

struct SinkState {
    std::vector<std::int16_t> samples;
    int closes{0};
};

class FakeSink final : public sdr::AudioSink {
public:
    explicit FakeSink(std::shared_ptr<SinkState> state) : state_(std::move(state)) {}
    void write(std::span<const std::int16_t> samples) override {
        state_->samples.insert(state_->samples.end(), samples.begin(), samples.end());
    }
    void close() override { ++state_->closes; }

private:
    std::shared_ptr<SinkState> state_;
};

class FakeFactory final : public sdr::AudioSinkFactory {
public:
    std::unique_ptr<sdr::AudioSink> create() override {
        states.push_back(std::make_shared<SinkState>());
        return std::make_unique<FakeSink>(states.back());
    }
    std::vector<std::shared_ptr<SinkState>> states;
};

TEST(SessionRecorder, IgnoresSilenceBeforeFirstSignal) {
    FakeFactory factory;
    sdr::SessionRecorder recorder(10, 1.0, factory);
    const std::vector<std::int16_t> audio(5, 1);
    recorder.process(audio, false);
    EXPECT_TRUE(factory.states.empty());
    EXPECT_FALSE(recorder.recording());
}

TEST(SessionRecorder, DiscardsTrailingSilenceAndClosesAtConfiguredDuration) {
    FakeFactory factory;
    sdr::SessionRecorder recorder(10, 1.0, factory);
    const std::vector<std::int16_t> signal(10, 2);
    const std::vector<std::int16_t> silence(5, 0);
    recorder.process(signal, true);
    recorder.process(silence, false);
    ASSERT_TRUE(recorder.recording());
    recorder.process(silence, false);

    ASSERT_EQ(factory.states.size(), 1U);
    EXPECT_EQ(factory.states[0]->samples.size(), 10U);
    EXPECT_EQ(factory.states[0]->samples[9], 2);
    EXPECT_EQ(factory.states[0]->closes, 1);
    EXPECT_FALSE(recorder.recording());
}

TEST(SessionRecorder, ContinuedSignalPreservesInternalPauseAndResetsTimer) {
    FakeFactory factory;
    sdr::SessionRecorder recorder(10, 1.0, factory);
    const std::vector<std::int16_t> signal(5, 2);
    const std::vector<std::int16_t> silence(5, 0);
    recorder.process(signal, true);
    recorder.process(silence, false);
    recorder.process(signal, true);
    recorder.process(silence, false);
    EXPECT_TRUE(recorder.recording());
    recorder.process(silence, false);
    EXPECT_FALSE(recorder.recording());
    ASSERT_EQ(factory.states.size(), 1U);
    ASSERT_EQ(factory.states[0]->samples.size(), 15U);
    EXPECT_EQ(factory.states[0]->samples[4], 2);
    EXPECT_EQ(factory.states[0]->samples[5], 0);
    EXPECT_EQ(factory.states[0]->samples[9], 0);
    EXPECT_EQ(factory.states[0]->samples[10], 2);
}

TEST(SessionRecorder, NewSignalAfterCloseCreatesNewFile) {
    FakeFactory factory;
    sdr::SessionRecorder recorder(10, 0.5, factory);
    const std::vector<std::int16_t> block(5, 1);
    recorder.process(block, true);
    ASSERT_EQ(factory.states.size(), 1U);
    EXPECT_TRUE(recorder.recording());
    recorder.process(block, false);
    EXPECT_FALSE(recorder.recording());
    EXPECT_EQ(factory.states[0]->closes, 1);
    recorder.process(block, true);
    EXPECT_EQ(factory.states.size(), 2U);
    EXPECT_TRUE(recorder.recording());
}

TEST(SessionRecorder, FinishDiscardsPendingTrailingSilence) {
    FakeFactory factory;
    sdr::SessionRecorder recorder(10, 1.0, factory);
    const std::vector<std::int16_t> signal(5, 2);
    const std::vector<std::int16_t> silence(5, 0);
    recorder.process(signal, true);
    recorder.process(silence, false);
    recorder.finish();

    ASSERT_EQ(factory.states.size(), 1U);
    EXPECT_EQ(factory.states[0]->samples, signal);
    EXPECT_EQ(factory.states[0]->closes, 1);
    EXPECT_FALSE(recorder.recording());
}

}  // namespace
