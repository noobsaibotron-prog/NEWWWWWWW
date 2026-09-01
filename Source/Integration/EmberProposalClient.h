#pragma once

#include "EmberProposalProtocol.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

namespace EmberProposal
{

class Transport
{
public:
    virtual ~Transport() = default;
    virtual bool send(const EncodedFrame& frame) = 0;
    virtual bool waitReceive(EncodedFrame& out, int timeoutMs) = 0;
    virtual void close() = 0;
    [[nodiscard]] virtual bool hasOpenListener() const { return false; }
    [[nodiscard]] virtual bool isConnected() const { return false; }
    virtual void setExpectMacTrailer(bool) {}
    /** Socket transports queue on send() and write from the I/O thread. */
    virtual bool flushOutbound() { return true; }
};

/** In-process duplex queue. No OS socket, no bind. Used by tests and as the
    E2 stand-in until ACB publishes a real loopback server (E3). */
class InProcessTransport final : public Transport
{
public:
    bool send(const EncodedFrame& frame) override;
    bool waitReceive(EncodedFrame& out, int timeoutMs) override;
    void close() override;
    [[nodiscard]] bool hasOpenListener() const override { return false; }
    [[nodiscard]] bool isConnected() const override { return !closed.load(); }

    void pushInbound(EncodedFrame frame);
    std::vector<EncodedFrame> takeOutbound();
    [[nodiscard]] std::size_t outboundCount() const;

private:
    mutable std::mutex mutex;
    std::condition_variable cv;
    std::deque<EncodedFrame> inbound;
    std::vector<EncodedFrame> outbound;
    std::atomic<bool> closed { false };
};

struct PairingState
{
    bool paired = false;
    bool offerPending = false;
    std::string runtimeInstanceId;
    std::string humanCode;
    std::string pairBindingId;
    std::uint64_t offerExpiresAtNs = 0;
};

class Client : private juce::Thread
{
public:
    using Delivery = std::function<void(WireMessage, EncodedFrame)>;

    Client();
    ~Client() override;

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const noexcept { return enabled.load(); }
    [[nodiscard]] bool hasOpenListener() const noexcept;
    [[nodiscard]] bool isIoThreadRunning() const noexcept { return isThreadRunning(); }

    void attachTransport(std::shared_ptr<Transport> transport);
    void setDelivery(Delivery delivery);
    void setClock(std::function<std::int64_t()> monotonicNs,
                  std::function<std::int64_t()> unixSeconds);

    void sendPairOffer(const PairOffer& offer);
    void sendUnpair(const UnpairCommand& command);
    void sendOutcome(const WireMessage& message);
    void sendHandshakeHello();
    void beginHandshakeWithRendezvous(const RendezvousRecord& record);
    void setControlDirectory(juce::File directory);
    [[nodiscard]] juce::File controlDirectory() const;
    void bindRendezvous(const juce::File& file, const RendezvousRecord& record);
    [[nodiscard]] juce::File boundRendezvousFile() const;

    void teardown(UnpairedCause cause);
    [[nodiscard]] std::uint64_t generation() const noexcept { return generation_.load(); }
    [[nodiscard]] bool isAuthenticated() const;
    [[nodiscard]] PairingState pairing() const;
    void notePairConfirm(const PairConfirm& confirm);
    void clearPairing();
    [[nodiscard]] ProposalSession& session() noexcept { return session_; }
    [[nodiscard]] const ProposalSession& session() const noexcept { return session_; }

    /** Test helper: inject a frame as if the I/O thread received it. */
    void injectInboundForTests(const EncodedFrame& frame);
    void tryConnectDefaultRendezvous();
    void setAutoConnectRendezvous(bool enabled) noexcept { autoConnectRendezvous = enabled; }

private:
    void run() override;
    void deliverOnMessageThread(WireMessage message, EncodedFrame frame);
    void handleInbound(const EncodedFrame& frame);
    std::int64_t nowNs() const;
    std::int64_t nowUnix() const;
    bool sendEncoded(const WireMessage& message, bool mac);
    [[nodiscard]] bool pairOfferAllowed() const;
    [[nodiscard]] juce::File activeRendezvousFile() const;

    std::atomic<bool> enabled { false };
    std::atomic<std::uint64_t> generation_ { 1 };
    static constexpr int kMaxPendingDeliveries = 8;

    std::shared_ptr<Transport> transport;
    Delivery delivery;
    std::function<std::int64_t()> monotonicNsFn;
    std::function<std::int64_t()> unixSecondsFn;

    mutable std::mutex stateMutex;
    ProposalSession session_;
    PairingState pairingState;
    std::array<std::uint8_t, 32> clientNonce {};
    bool handshakeStarted = false;

    struct Lifetime
    {
        std::atomic<bool> alive { true };
        std::atomic<std::uint64_t> generation { 1 };
        std::atomic<int> pendingDeliveries { 0 };
    };
    std::shared_ptr<Lifetime> lifetime;
    bool autoConnectRendezvous = false;
    juce::File controlDirectory_;
    juce::File boundRendezvousFile_;
    std::string boundSessionUuid;
    std::string boundSessionSecretHex;
    std::int64_t lastRendezvousAttemptMs = 0;
};

[[nodiscard]] juce::File defaultControlDirectory();
[[nodiscard]] juce::File defaultRendezvousFile();
[[nodiscard]] bool controlDirectoryIsUsable(const juce::File& directory, ProtocolErrorCode& error);
[[nodiscard]] bool rendezvousFileIsUsable(const juce::File& file, ProtocolErrorCode& error);
[[nodiscard]] std::optional<RendezvousRecord> readRendezvousFile(const juce::File& file,
                                                                 std::int64_t nowUnixS,
                                                                 ProtocolErrorCode& error);

} // namespace EmberProposal
