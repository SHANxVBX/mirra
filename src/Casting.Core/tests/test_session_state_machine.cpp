#include <gtest/gtest.h>
#include "session/SessionStateMachine.h"
#include "ipc/IPipeServer.h"
#include "ipc/IpcMessages.h"
#include <vector>

using namespace mirra;

// Mock IPipeServer to capture sent IPC messages during state machine testing
class MockPipeServer : public IPipeServer {
public:
    bool start() override { return true; }
    void stop() override {}
    void onMessage(MessageCallback cb) override { m_cb = std::move(cb); }
    void send(const IpcMessage& msg) override { sent.push_back(msg); }
    bool isRunning() const override { return true; }

    std::vector<IpcMessage> sent;
    MessageCallback m_cb;
};

class SessionStateMachineTest : public ::testing::Test {
protected:
    std::string sessionId = "test-session-001";
    std::string deviceSerial = "emulator-5554";
    MockPipeServer mockPipe;
    std::unique_ptr<SessionStateMachine> sm;

    void SetUp() override {
        sm = std::make_unique<SessionStateMachine>(sessionId, deviceSerial, mockPipe);
    }

    void TearDown() override {
        sm.reset();
    }
};

TEST_F(SessionStateMachineTest, InitialStateIsIdle) {
    EXPECT_EQ(sm->currentState(), SessionState::Idle);
}

TEST_F(SessionStateMachineTest, StartSessionCommandTransitionsFromIdleToAdbSetup) {
    IpcMessage cmd;
    cmd.type = CMD_START_SESSION;
    sm->handleCommand(cmd);
    // After handleCommand, state transitions to AdbSetup immediately
    EXPECT_EQ(sm->currentState(), SessionState::AdbSetup);
}

TEST_F(SessionStateMachineTest, StopSessionFromAnyStateSetsStoppedState) {
    IpcMessage start, stop;
    start.type = CMD_START_SESSION;
    stop.type  = CMD_STOP_SESSION;

    sm->handleCommand(start);
    sm->handleCommand(stop);
    EXPECT_TRUE(sm->isStopped());
}

TEST_F(SessionStateMachineTest, StateChangedEventIsSentOnTransition) {
    IpcMessage cmd;
    cmd.type = CMD_START_SESSION;
    sm->handleCommand(cmd);

    // Should have sent a StateChanged event
    ASSERT_FALSE(mockPipe.sent.empty());
    EXPECT_EQ(mockPipe.sent.back().type, std::string(MSG_STATE_CHANGED));
}

TEST_F(SessionStateMachineTest, IgnoresStartSessionIfAlreadyInAdbSetup) {
    IpcMessage cmd;
    cmd.type = CMD_START_SESSION;
    sm->handleCommand(cmd); // → AdbSetup
    auto stateAfterFirst = sm->currentState();
    sm->handleCommand(cmd); // should be ignored
    EXPECT_EQ(sm->currentState(), stateAfterFirst);
}
