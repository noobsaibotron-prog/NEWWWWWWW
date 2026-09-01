#include "EmberProposalClient.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace EmberProposal
{

bool InProcessTransport::send(const EncodedFrame& frame)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (closed.load())
        return false;
    outbound.push_back(frame);
    return true;
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

    bool send(const EncodedFrame& frame) override
    {
        if (socket == nullptr || !connected)
            return false;
        const auto len = juce::ByteOrder::swapIfLittleEndian(
            static_cast<std::uint32_t>(frame.body.size()));
        if (socket->waitUntilReady(false, 200) < 0)
            return false;
        if (socket->write(&len, 4) != 4)
            return false;
        if (!frame.body.empty()
            && socket->write(frame.body.data(), static_cast<int>(frame.body.size()))
                   != static_cast<int>(frame.body.size()))
            return false;
        if (frame.hasHmac && socket->write(frame.hmac.data(), 32) != 32)
            return false;
        return true;
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
        out.hasHmac = false;
        if (!readExact(out.body.data(), static_cast<int>(len)))
            return false;
        if (expectMacTrailer.load())
        {
            std::uint8_t mac[32];
            if (!readExact(mac, 32))
                return false;
            std::memcpy(out.hmac.data(), mac, 32);
            out.hasHmac = true;
        }
        return true;
    }

    void setExpectMacTrailer(bool expectMac) override
    {
        expectMacTrailer.store(expectMac);
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
    std::atomic<bool> expectMacTrailer { false };
};
} // namespace

Client::Client()
    : juce::Thread("Ember Proposal Client"),
      lifetime(std::make_shared<Lifetime>())
{
    monotonicNsFn = defaultMonotonicNs;
    unixSecondsFn = defaultUnixSeconds;
    controlDirectory_ = defaultControlDirectory();
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
    if (!pairOfferAllowed())
        return;

    PairOffer clamped = offer;
    const auto now = static_cast<std::uint64_t>(std::max<std::int64_t>(0, nowNs()));
    const auto cap = static_cast<std::uint64_t>(kMaxSafeJsonInt);
    if (clamped.expiresAtMonotonicNs > cap || clamped.expiresAtMonotonicNs < now)
        clamped.expiresAtMonotonicNs = std::min(now + kPairOfferTtlNs, cap);

    WireMessage message;
    message.type = MessageType::pair_offer;
    message.pairOffer = clamped;
    const bool mac = session_.isAuthenticated();
    const bool bound = !boundRendezvousFile_.getFullPathName().isEmpty();
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        pairingState.offerPending = true;
        pairingState.runtimeInstanceId = clamped.runtimeInstanceId;
        pairingState.humanCode = clamped.humanCode;
        pairingState.offerExpiresAtNs = clamped.expiresAtMonotonicNs;
        pairingState.paired = false;
        pairingState.pairBindingId.clear();
    }
    if (!sendEncoded(message, mac) && bound)
        clearPairing();
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

void Client::setControlDirectory(juce::File directory)
{
    std::lock_guard<std::mutex> lock(stateMutex);
    controlDirectory_ = std::move(directory);
}

juce::File Client::controlDirectory() const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return controlDirectory_;
}

void Client::bindRendezvous(const juce::File& file, const RendezvousRecord& record)
{
    std::lock_guard<std::mutex> lock(stateMutex);
    boundRendezvousFile_ = file;
    boundSessionUuid = record.sessionUuid;
    boundSessionSecretHex = record.sessionSecretHex;
}

juce::File Client::boundRendezvousFile() const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return boundRendezvousFile_;
}

juce::File Client::activeRendezvousFile() const
{
    return controlDirectory_.getChildFile(kRendezvousFileName);
}

bool Client::pairOfferAllowed() const
{
    juce::File bound;
    juce::File control;
    std::string uuid;
    std::string secret;
    bool authed = false;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (!enabled.load())
            return false;
        if (boundRendezvousFile_.getFullPathName().isEmpty())
            return true;
        authed = session_.isAuthenticated();
        bound = boundRendezvousFile_;
        control = controlDirectory_;
        uuid = boundSessionUuid;
        secret = boundSessionSecretHex;
    }
    if (!authed)
        return false;
    if (bound.getParentDirectory().getFullPathName() != control.getFullPathName())
        return false;
    ProtocolErrorCode error = ProtocolErrorCode::stale_rendezvous;
    auto record = readRendezvousFile(bound, nowUnix(), error);
    if (!record)
        return false;
    return record->sessionUuid == uuid && record->sessionSecretHex == secret;
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
        bool authed = false;
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            t = transport;
            authed = session_.isAuthenticated();
        }

        if (t != nullptr)
        {
            t->setExpectMacTrailer(authed);
            EncodedFrame frame;
            if (t->waitReceive(frame, 50))
                handleInbound(frame);
        }
        else
        {
            if (enabled.load() && autoConnectRendezvous)
            {
                const auto nowMs = static_cast<std::int64_t>(juce::Time::getMillisecondCounter());
                if (nowMs - lastRendezvousAttemptMs >= 250)
                {
                    lastRendezvousAttemptMs = nowMs;
                    tryConnectDefaultRendezvous();
                }
            }
            juce::Thread::sleep(50);
        }
    }
}

void Client::tryConnectDefaultRendezvous()
{
    if (std::getenv("EMBER_PROPOSAL_DISABLE_AUTOCONNECT") != nullptr)
        return;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (transport != nullptr && transport->isConnected())
            return;
    }
    ProtocolErrorCode error = ProtocolErrorCode::stale_rendezvous;
    const auto file = activeRendezvousFile();
    auto record = readRendezvousFile(file, nowUnix(), error);
    if (!record)
        return;
    auto loop = std::make_shared<LoopbackClientTransport>(*record);
    if (!loop->connect(400))
        return;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        transport = loop;
        boundRendezvousFile_ = file;
        boundSessionUuid = record->sessionUuid;
        boundSessionSecretHex = record->sessionSecretHex;
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
        if (transport)
            transport->setExpectMacTrailer(true);
    }

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

bool Client::sendEncoded(const WireMessage& message, bool mac)
{
    std::lock_guard<std::mutex> lock(stateMutex);
    auto encoded = session_.encode(message, nowNs(), mac);
    if (!encoded.ok || transport == nullptr)
        return false;
    return transport->send(encoded.frame);
}

namespace
{
bool lstatPath(const juce::File& file, struct stat& st)
{
    return lstat(file.getFullPathName().toRawUTF8(), &st) == 0;
}
} // namespace

juce::File defaultControlDirectory()
{
#if JUCE_MAC
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Application Support")
        .getChildFile("AbletonCopilotBridge")
        .getChildFile("control");
#else
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("AbletonCopilotBridge")
        .getChildFile("control");
#endif
}

juce::File defaultRendezvousFile()
{
    return defaultControlDirectory().getChildFile(kRendezvousFileName);
}

bool controlDirectoryIsUsable(const juce::File& directory, ProtocolErrorCode& error)
{
    struct stat st {};
    if (!lstatPath(directory, st))
    {
        error = ProtocolErrorCode::stale_rendezvous;
        return false;
    }
    if (S_ISLNK(st.st_mode) || !S_ISDIR(st.st_mode) || st.st_uid != getuid()
        || (st.st_mode & 0777) != 0700)
    {
        error = ProtocolErrorCode::stale_rendezvous;
        return false;
    }
    return true;
}

bool rendezvousFileIsUsable(const juce::File& file, ProtocolErrorCode& error)
{
    struct stat st {};
    if (!lstatPath(file, st))
    {
        error = ProtocolErrorCode::stale_rendezvous;
        return false;
    }
    if (S_ISLNK(st.st_mode) || !S_ISREG(st.st_mode) || st.st_uid != getuid()
        || (st.st_mode & 0777) != 0600)
    {
        error = ProtocolErrorCode::stale_rendezvous;
        return false;
    }
    if (!controlDirectoryIsUsable(file.getParentDirectory(), error))
        return false;
    return true;
}

std::optional<RendezvousRecord> readRendezvousFile(const juce::File& file,
                                                   std::int64_t nowUnixS,
                                                   ProtocolErrorCode& error)
{
    if (!rendezvousFileIsUsable(file, error))
        return std::nullopt;
    const auto text = file.loadFileAsString().toStdString();
    auto record = parseRendezvousJson(text, error);
    if (!record)
        return std::nullopt;
    if (!validateRendezvous(*record, nowUnixS, error))
        return std::nullopt;
    return record;
}

} // namespace EmberProposal
