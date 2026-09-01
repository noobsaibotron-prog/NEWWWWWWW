#include "EmberProposalClient.h"

#include <chrono>
#include <cstring>
#include <utility>

namespace EmberProposal
{

void InProcessTransport::send(const EncodedFrame& frame)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (closed.load())
        return;
    outbound.push_back(frame);
}

bool InProcessTransport::waitReceive(EncodedFrame& out, int timeoutMs)
{
    std::unique_lock<std::mutex> lock(mutex);
    if (cv.wait_for(lock, std::chrono::milliseconds(timeoutMs), [&] {
            return closed.load() || !inbound.empty();
        }))
    {
        if (closed.load() && inbound.empty())
            return false;
        if (inbound.empty())
            return false;
        out = std::move(inbound.front());
        inbound.pop_front();
        return true;
    }
    return false;
}

void InProcessTransport::close()
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        closed.store(true);
    }
    cv.notify_all();
}

void InProcessTransport::pushInbound(EncodedFrame frame)
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (closed.load())
            return;
        inbound.push_back(std::move(frame));
    }
    cv.notify_one();
}

std::vector<EncodedFrame> InProcessTransport::takeOutbound()
{
    std::lock_guard<std::mutex> lock(mutex);
    auto copy = outbound;
    outbound.clear();
    return copy;
}

std::size_t InProcessTransport::outboundCount() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return outbound.size();
}

namespace
{
std::int64_t defaultMonotonicNs()
{
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

std::int64_t defaultUnixSeconds()
{
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

/** Outbound loopback client. Ember never binds. Frame layout after Link ON:
    uint32_be json_len | json | [32-byte HMAC if authenticated]. Handshake
    frames omit HMAC. E3 owns the matching ACB server. */
class LoopbackClientTransport final : public Transport
{
public:
    explicit LoopbackClientTransport(const RendezvousRecord& record)
        : host(record.listenAddress), port(record.listenPort)
    {
    }

    ~LoopbackClientTransport() override { close(); }

    bool connect(int timeoutMs)
    {
        if (host != kLoopbackAddress)
            return false;
        socket = std::make_unique<juce::StreamingSocket>();
        if (!socket->connect(host, port, timeoutMs))
        {
            socket.reset();
            return false;
        }
        connected = true;
        return true;
    }

    void send(const EncodedFrame& frame) override
    {
        if (socket == nullptr || !connected)
            return;
        const auto len = juce::ByteOrder::swapIfLittleEndian(
            static_cast<std::uint32_t>(frame.body.size()));
        if (socket->waitUntilReady(false, 200) < 0)
            return;
        socket->write(&len, 4);
        if (!frame.body.empty())
            socket->write(frame.body.data(), static_cast<int>(frame.body.size()));
        if (frame.hasHmac)
            socket->write(frame.hmac.data(), 32);
    }

    bool waitReceive(EncodedFrame& out, int timeoutMs) override
    {
        if (socket == nullptr || !connected)
        {
            juce::Thread::sleep(std::min(timeoutMs, 50));
            return false;
        }
        if (socket->waitUntilReady(true, timeoutMs) <= 0)
            return false;

        std::uint32_t len = 0;
        if (!readExact(&len, 4))
            return false;
        len = juce::ByteOrder::swapIfLittleEndian(len);
        if (len == 0 || len > static_cast<std::uint32_t>(kMaxFrameBytes))
            return false;
        out.body.resize(len);
        if (!readExact(out.body.data(), static_cast<int>(len)))
            return false;
        // HMAC trailer is present once the peer has authenticated; E2 treats
        // a following 32 bytes as HMAC when readable without blocking long.
        if (socket->waitUntilReady(true, 0) > 0)
        {
            std::uint8_t mac[32];
            const int got = socket->read(mac, 32, false);
            if (got == 32)
            {
                std::memcpy(out.hmac.data(), mac, 32);
                out.hasHmac = true;
            }
        }
        return true;
    }

    void close() override
    {
        connected = false;
        if (socket != nullptr)
        {
            socket->close();
            socket.reset();
        }
    }

    [[nodiscard]] bool hasOpenListener() const override { return false; }
    [[nodiscard]] bool isConnected() const override { return connected; }

private:
    bool readExact(void* dest, int n)
    {
        auto* p = static_cast<char*>(dest);
        int got = 0;
        while (got < n)
        {
            const int r = socket->read(p + got, n - got, true);
            if (r <= 0)
                return false;
            got += r;
        }
        return true;
    }

    juce::String host;
    int port = 0;
    std::unique_ptr<juce::StreamingSocket> socket;
    bool connected = false;
};
} // namespace

Client::Client()
    : juce::Thread("Ember Proposal Client"),
      lifetime(std::make_shared<Lifetime>())
{
    monotonicNsFn = defaultMonotonicNs;
    unixSecondsFn = defaultUnixSeconds;
}

Client::~Client()
{
    teardown(UnpairedCause::disconnect);
}

void Client::setEnabled(bool shouldEnable)
{
    if (shouldEnable == enabled.load())
        return;

    if (!shouldEnable)
    {
        teardown(UnpairedCause::ember_link_disabled);
        enabled.store(false);
        return;
    }

    enabled.store(true);
    generation_.fetch_add(1, std::memory_order_acq_rel);
    if (lifetime)
        lifetime->generation.store(generation_.load(std::memory_order_acquire),
                                   std::memory_order_release);
    handshakeStarted = false;
    if (autoConnectRendezvous)
        tryConnectDefaultRendezvous();
    if (!isThreadRunning())
        startThread(juce::Thread::Priority::background);
}

bool Client::hasOpenListener() const noexcept
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return transport != nullptr && transport->hasOpenListener();
}

void Client::attachTransport(std::shared_ptr<Transport> next)
{
    std::lock_guard<std::mutex> lock(stateMutex);
    transport = std::move(next);
}

void Client::setDelivery(Delivery next)
{
    delivery = std::move(next);
}

void Client::setClock(std::function<std::int64_t()> monotonicNs,
                      std::function<std::int64_t()> unixSeconds)
{
    if (monotonicNs)
        monotonicNsFn = std::move(monotonicNs);
    if (unixSeconds)
        unixSecondsFn = std::move(unixSeconds);
}

void Client::sendPairOffer(const PairOffer& offer)
{
    WireMessage message;
    message.type = MessageType::pair_offer;
    message.pairOffer = offer;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        pairingState.offerPending = true;
        pairingState.runtimeInstanceId = offer.runtimeInstanceId;
        pairingState.humanCode = offer.humanCode;
        pairingState.offerExpiresAtNs = offer.expiresAtMonotonicNs;
        pairingState.paired = false;
        pairingState.pairBindingId.clear();
    }
    sendEncoded(message, session_.isAuthenticated());
}

void Client::sendUnpair(const UnpairCommand& command)
{
    WireMessage message;
    message.type = MessageType::unpair;
    message.unpair = command;
    sendEncoded(message, session_.isAuthenticated());
    clearPairing();
}

void Client::sendOutcome(const WireMessage& message)
{
    sendEncoded(message, session_.isAuthenticated());
}

void Client::sendHandshakeHello()
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (!session_.hasSecret())
        return;
    fillCsprng(clientNonce.data(), clientNonce.size());
    session_.clientNonce = clientNonce;
    session_.hasClientNonce = true;
    session_.lastClientNonceHex = toHex(clientNonce.data(), clientNonce.size());

    WireMessage hello;
    hello.type = MessageType::handshake;
    hello.handshakePhase = HandshakePhase::hello;
    hello.hello.clientNonceHex = session_.lastClientNonceHex;
    hello.hello.capabilities = { "proposal" };
    handshakeStarted = true;
    // encode while holding mutex would deadlock sendEncoded — drop lock via copy
    auto copy = hello;
    auto ns = nowNs();
    auto encoded = session_.encode(copy, ns, false);
    if (encoded.ok && transport != nullptr)
        transport->send(encoded.frame);
}

void Client::beginHandshakeWithRendezvous(const RendezvousRecord& record)
{
    ProtocolErrorCode error = ProtocolErrorCode::stale_rendezvous;
    if (!validateRendezvous(record, nowUnix(), error))
        return;
    bool secretOk = false;
    const auto secret = secretFromHex(record.sessionSecretHex, secretOk);
    if (!secretOk)
        return;
    session_.setSecret(secret);
    sendHandshakeHello();
}

void Client::teardown(UnpairedCause /*cause*/)
{
    enabled.store(false);
    generation_.fetch_add(1, std::memory_order_acq_rel);
    if (lifetime)
    {
        lifetime->alive.store(false, std::memory_order_release);
        lifetime->generation.store(generation_.load(std::memory_order_acquire));
    }
    signalThreadShouldExit();
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (transport)
            transport->close();
    }
    stopThread(2000);
    clearPairing();
    session_.reset();
    handshakeStarted = false;
    lifetime = std::make_shared<Lifetime>();
    lifetime->generation.store(generation_.load(std::memory_order_acquire));
}

bool Client::isAuthenticated() const
{
    return session_.isAuthenticated();
}

PairingState Client::pairing() const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return pairingState;
}

void Client::notePairConfirm(const PairConfirm& confirm)
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (!pairingState.offerPending
        || pairingState.humanCode.empty()
        || pairingState.runtimeInstanceId.empty()
        || confirm.humanCode != pairingState.humanCode
        || confirm.runtimeInstanceId != pairingState.runtimeInstanceId)
        return;
    pairingState.paired = true;
    pairingState.offerPending = false;
    pairingState.pairBindingId = confirm.pairBindingId;
    pairingState.runtimeInstanceId = confirm.runtimeInstanceId;
    pairingState.humanCode = confirm.humanCode;
}

void Client::clearPairing()
{
    std::lock_guard<std::mutex> lock(stateMutex);
    pairingState = {};
}

void Client::injectInboundForTests(const EncodedFrame& frame)
{
    handleInbound(frame);
}

void Client::run()
{
    while (!threadShouldExit())
    {
        std::shared_ptr<Transport> t;
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            t = transport;
        }

        if (t != nullptr)
        {
            EncodedFrame frame;
            if (t->waitReceive(frame, 50))
                handleInbound(frame);
        }
        else
        {
            juce::Thread::sleep(50);
        }
    }
}

void Client::tryConnectDefaultRendezvous()
{
    ProtocolErrorCode error = ProtocolErrorCode::stale_rendezvous;
    auto record = readRendezvousFile(defaultRendezvousFile(), nowUnix(), error);
    if (!record)
        return;
    auto loop = std::make_shared<LoopbackClientTransport>(*record);
    if (!loop->connect(400))
        return;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        transport = loop;
    }
    beginHandshakeWithRendezvous(*record);
}

void Client::handleInbound(const EncodedFrame& frame)
{
    WireMessage outboundConfirm;
    bool sendConfirm = false;
    IngestResult ingested;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        ingested = session_.ingest(frame);
        if (!ingested.ok)
        {
            WireMessage err;
            err.type = MessageType::protocol_error;
            err.protocolError.protocolErrorCode = ingested.error;
            err.protocolError.closeConnection = true;
            err.protocolError.audit = makeAudit(frame.body.data(), frame.body.size());
            auto encoded = session_.encode(err, nowNs(), session_.isAuthenticated());
            if (encoded.ok && transport != nullptr)
                transport->send(encoded.frame);
            session_.setAuthenticated(false);
            if (transport)
                transport->close();
            return;
        }

        if (ingested.message.type == MessageType::handshake
            && ingested.message.handshakePhase == HandshakePhase::ack)
        {
            HandshakeConfirm confirmPayload;
            if (!session_.applyClientAck(ingested.message.ack, confirmPayload))
            {
                WireMessage err;
                err.type = MessageType::protocol_error;
                err.protocolError.protocolErrorCode = ProtocolErrorCode::hmac_mismatch;
                err.protocolError.closeConnection = true;
                err.protocolError.audit = makeAudit(frame.body.data(), frame.body.size());
                auto encoded = session_.encode(err, nowNs(), false);
                if (encoded.ok && transport != nullptr)
                    transport->send(encoded.frame);
                if (transport)
                    transport->close();
                return;
            }
            outboundConfirm.type = MessageType::handshake;
            outboundConfirm.handshakePhase = HandshakePhase::confirm;
            outboundConfirm.confirm = confirmPayload;
            sendConfirm = true;
        }
    }

    if (sendConfirm)
    {
        sendEncoded(outboundConfirm, false);
        session_.setAuthenticated(true);
    }

    if (ingested.message.type == MessageType::handshake)
        return;

    deliverOnMessageThread(std::move(ingested.message), frame);
}

void Client::deliverOnMessageThread(WireMessage message, EncodedFrame frame)
{
    auto life = lifetime;
    if (life == nullptr)
        return;
    auto cb = delivery;
    if (juce::MessageManager::existsAndIsCurrentThread())
    {
        if (!life->alive.load(std::memory_order_acquire))
            return;
        if (cb)
            cb(std::move(message), std::move(frame));
        return;
    }
    if (life->pendingDeliveries.load(std::memory_order_acquire) >= kMaxPendingDeliveries)
        return;
    life->pendingDeliveries.fetch_add(1, std::memory_order_acq_rel);
    const auto gen = generation_.load(std::memory_order_acquire);
    juce::MessageManager::callAsync(
        [life, gen, cb, message = std::move(message), frame = std::move(frame)]() mutable
        {
            life->pendingDeliveries.fetch_sub(1, std::memory_order_acq_rel);
            if (!life->alive.load(std::memory_order_acquire) || life->generation.load() != gen)
                return;
            if (!juce::MessageManager::existsAndIsCurrentThread())
                return;
            if (cb)
                cb(std::move(message), std::move(frame));
        });
}

std::int64_t Client::nowNs() const
{
    return monotonicNsFn ? monotonicNsFn() : defaultMonotonicNs();
}

std::int64_t Client::nowUnix() const
{
    return unixSecondsFn ? unixSecondsFn() : defaultUnixSeconds();
}

void Client::sendEncoded(const WireMessage& message, bool mac)
{
    std::lock_guard<std::mutex> lock(stateMutex);
    auto encoded = session_.encode(message, nowNs(), mac);
    if (encoded.ok && transport != nullptr)
        transport->send(encoded.frame);
}

juce::File defaultRendezvousFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("AbletonCopilotBridge")
        .getChildFile("control")
        .getChildFile("ember-proposal-v1.json");
}

std::optional<RendezvousRecord> readRendezvousFile(const juce::File& file,
                                                   std::int64_t nowUnixS,
                                                   ProtocolErrorCode& error)
{
    if (!file.existsAsFile())
    {
        error = ProtocolErrorCode::stale_rendezvous;
        return std::nullopt;
    }
    const auto text = file.loadFileAsString().toStdString();
    auto record = parseRendezvousJson(text, error);
    if (!record)
        return std::nullopt;
    if (!validateRendezvous(*record, nowUnixS, error))
        return std::nullopt;
    return record;
}

} // namespace EmberProposal
