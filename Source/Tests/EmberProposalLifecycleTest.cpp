#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "../Integration/EmberProposalClient.h"
#include "../Integration/EmberProposalProtocol.h"
#include "../Integration/ExternalSemanticProposalInbox.h"
#include "../PluginProcessor.h"
#include "../GUI/SemanticControlPanel.h"

#include <CoreFoundation/CoreFoundation.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <cstdlib>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace
{
using namespace EmberProposal;

struct FakeAcb
{
    std::array<std::uint8_t, 32> secret {};
    ProposalSession session;
    std::int64_t now = 1;

    FakeAcb()
    {
        fillCsprng(secret.data(), secret.size());
        session.setSecret(secret);
    }

    EncodedFrame ackHello(const EncodedFrame& helloFrame)
    {
        auto ingested = session.ingest(helloFrame);
        if (!ingested.ok || ingested.message.type != MessageType::handshake)
            return {};
        std::uint8_t clientNonce[32];
        if (!fromHex(ingested.message.hello.clientNonceHex, clientNonce, 32))
            return {};
        std::uint8_t serverNonce[32];
        fillCsprng(serverNonce, 32);
        std::uint8_t concat[64];
        std::memcpy(concat, clientNonce, 32);
        std::memcpy(concat + 32, serverNonce, 32);
        std::uint8_t mac[32];
        hmacSha256(secret.data(), secret.size(), concat, 64, mac);

        WireMessage ack;
        ack.type = MessageType::handshake;
        ack.handshakePhase = HandshakePhase::ack;
        ack.ack.clientNonceHex = ingested.message.hello.clientNonceHex;
        ack.ack.serverNonceHex = toHex(serverNonce, 32);
        ack.ack.hmacSha256Hex = toHex(mac, 32);
        ack.ack.sessionUuid = randomUuidV4();
        ack.ack.serverEpoch = 1;
        return session.encode(ack, now++, false).frame;
    }

    EncodedFrame confirmAndAuth(const EncodedFrame& confirmFrame)
    {
        auto ingested = session.ingest(confirmFrame);
        if (ingested.ok && ingested.message.handshakePhase == HandshakePhase::confirm)
            session.setAuthenticated(true);
        return {};
    }

    EncodedFrame stage(const StageSemanticRequest& req)
    {
        WireMessage m;
        m.type = MessageType::stage_semantic_request;
        m.stage = req;
        return session.encode(m, now++, true).frame;
    }

    EncodedFrame pairConfirm(const PairConfirm& confirm)
    {
        WireMessage m;
        m.type = MessageType::pair_confirm;
        m.pairConfirm = confirm;
        return session.encode(m, now++, true).frame;
    }
};

void pump(int ms)
{
    const CFTimeInterval until = CFAbsoluteTimeGetCurrent() + ms / 1000.0;
    while (CFAbsoluteTimeGetCurrent() < until)
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, true);
}

/** APVTS digest of everything Semantic APPLY is allowed to write. Staging a
    Copilot phrase must not move this; inbound has no applySemanticAdjustments. */
juce::String eqDigest(AIEqualizerAudioProcessor& proc)
{
    juce::String d;
    if (auto* n = proc.getAPVTS().getRawParameterValue("numActiveBands"))
        d << "n=" << juce::String(n->load(), 4) << ";";
    for (int i = 0; i < AIEqualizerAudioProcessor::maxBands; ++i)
    {
        const auto b = proc.getBandState(i);
        d << i << ":"
          << juce::String(b.frequency, 2) << ","
          << juce::String(b.gain, 4) << ","
          << juce::String(b.q, 4) << ","
          << b.type << ","
          << (b.enabled ? 1 : 0) << ";";
    }
    return d;
}

WireMessage pairConfirmMatching(const LinkUiState& offer)
{
    WireMessage m;
    m.type = MessageType::pair_confirm;
    m.pairConfirm.pairBindingId = randomUuidV4();
    m.pairConfirm.runtimeInstanceId = offer.runtimeInstanceId;
    m.pairConfirm.humanCode = offer.humanCode;
    m.pairConfirm.controlRevision = 1;
    m.pairConfirm.confirmedAtMonotonicNs = 2;
    return m;
}

bool readExactSocket(juce::StreamingSocket& socket, void* dest, int n)
{
    auto* p = static_cast<char*>(dest);
    int got = 0;
    while (got < n)
    {
        const int r = socket.read(p + got, n - got, true);
        if (r <= 0)
            return false;
        got += r;
    }
    return true;
}

bool writeTcpFrame(juce::StreamingSocket& socket, const EncodedFrame& frame)
{
    const auto len = juce::ByteOrder::swapIfLittleEndian(
        static_cast<std::uint32_t>(frame.body.size()));
    if (socket.write(&len, 4) != 4)
        return false;
    if (!frame.body.empty()
        && socket.write(frame.body.data(), static_cast<int>(frame.body.size()))
               != static_cast<int>(frame.body.size()))
        return false;
    if (frame.hasHmac && socket.write(frame.hmac.data(), 32) != 32)
        return false;
    return true;
}

bool readTcpFrame(juce::StreamingSocket& socket, EncodedFrame& out, bool mac, int timeoutMs)
{
    if (socket.waitUntilReady(true, timeoutMs) <= 0)
        return false;
    std::uint32_t len = 0;
    if (!readExactSocket(socket, &len, 4))
        return false;
    len = juce::ByteOrder::swapIfLittleEndian(len);
    if (len == 0 || len > static_cast<std::uint32_t>(kMaxFrameBytes))
        return false;
    out.body.resize(len);
    out.hasHmac = false;
    if (!readExactSocket(socket, out.body.data(), static_cast<int>(len)))
        return false;
    if (mac)
    {
        if (!readExactSocket(socket, out.hmac.data(), 32))
            return false;
        out.hasHmac = true;
    }
    return true;
}

std::optional<PairOffer> takeLastPairOffer(InProcessTransport& pipe)
{
    std::optional<PairOffer> found;
    for (const auto& frame : pipe.takeOutbound())
    {
        const std::string body(frame.body.begin(), frame.body.end());
        const auto parsed = parseWireJson(body);
        if (parsed.ok && parsed.message.type == MessageType::pair_offer)
            found = parsed.message.pairOffer;
    }
    return found;
}

juce::File makeControlDir(const juce::String& tag)
{
    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getChildFile("ember-pair-rdv")
                   .getChildFile(tag + "-" + juce::Uuid().toDashedString())
                   .getChildFile("control");
    dir.createDirectory();
    chmod(dir.getFullPathName().toRawUTF8(), 0700);
    return dir;
}

RendezvousRecord writeUsableRendezvous(const juce::File& control)
{
    RendezvousRecord record;
    record.listenAddress = kLoopbackAddress;
    record.listenPort = 23456;
    record.sessionUuid = randomUuidV4();
    record.sessionSecretHex = randomHex64();
    record.serverEpoch = 1;
    record.expiresAtUnixS = 2'000'000'000;
    record.createdAtUnixS = 1;
    const auto file = control.getChildFile(kRendezvousFileName);
    file.replaceWithText(serializeRendezvousJson(record));
    chmod(file.getFullPathName().toRawUTF8(), 0600);
    return record;
}

WireMessage stageMoreAir(const LinkUiState& paired, const PairOffer& offer)
{
    WireMessage m;
    m.type = MessageType::stage_semantic_request;
    m.stage.targetRuntimeInstanceId = paired.runtimeInstanceId;
    m.stage.pairBindingId = paired.pairBindingId;
    m.stage.requestId = randomUuidV4();
    m.stage.expectedControlRevision = offer.controlRevision;
    m.stage.expectedProjectionBaseEpoch = offer.projectionBaseEpoch;
    m.stage.expectedAuditionContextEpoch = offer.auditionContextEpoch;
    m.stage.phrase = "more air";
    m.stage.intensity = 0.4;
    m.stage.expiresAtMonotonicNs = static_cast<std::uint64_t>(
        std::max<std::int64_t>(0, juce::Time::getHighResolutionTicks())) + 60'000'000'000ULL;
    m.stage.requestHash = requestHashHex(m.stage);
    return m;
}

class EmberProposalLifecycleTest : public juce::UnitTest
{
public:
    EmberProposalLifecycleTest()
        : juce::UnitTest("Ember Proposal Lifecycle (E2)", "Integration") {}

    void runTest() override
    {
        beginTest("message thread ownership");
        expect(juce::MessageManager::existsAndIsCurrentThread());

        beginTest("default client opens no listener");
        {
            Client client;
            expect(!client.isEnabled());
            expect(!client.hasOpenListener());
            expect(!client.isIoThreadRunning());
        }

        beginTest("disabled processor does not open a listener");
        {
            AIEqualizerAudioProcessor proc;
            expect(!proc.isEmberLinkEnabled());
            expect(!proc.emberProposalHasOpenListener());
            expect(!proc.isEmberEditorOpen());
        }

        beginTest("handshake pair and stage via in-process transport");
        {
            auto pipe = std::make_shared<InProcessTransport>();
            Client client;
            client.setAutoConnectRendezvous(false);
            client.attachTransport(pipe);
            std::atomic<int> deliveries { 0 };
            WireMessage last;
            client.setDelivery([&](WireMessage m, EncodedFrame) {
                expect(juce::MessageManager::existsAndIsCurrentThread());
                last = std::move(m);
                deliveries.fetch_add(1);
            });

            FakeAcb acb;
            client.session().setSecret(acb.secret);
            client.setEnabled(true);
            client.sendHandshakeHello();
            pump(20);

            auto outbound = pipe->takeOutbound();
            expect(outbound.size() >= 1);
            auto ack = acb.ackHello(outbound.front());
            pipe->pushInbound(ack);
            pump(80);
            outbound = pipe->takeOutbound();
            expect(!outbound.empty());
            acb.confirmAndAuth(outbound.back());
            expect(client.isAuthenticated());

            PairOffer offer;
            offer.runtimeInstanceId = randomUuidV4();
            offer.humanCode = randomHumanCode();
            offer.controlRevision = 1;
            offer.projectionBaseEpoch = 1;
            offer.auditionContextEpoch = 1;
            offer.expiresAtMonotonicNs = 10'000'000'000ULL;
            offer.editorOpen = true;
            client.sendPairOffer(offer);
            outbound = pipe->takeOutbound();
            expect(!outbound.empty());

            PairConfirm confirm;
            confirm.pairBindingId = randomUuidV4();
            confirm.runtimeInstanceId = offer.runtimeInstanceId;
            confirm.humanCode = offer.humanCode;
            confirm.controlRevision = 1;
            confirm.confirmedAtMonotonicNs = 2;
            confirm.liveIdentity.trackId = "1";
            confirm.liveIdentity.deviceId = "2";
            confirm.liveIdentity.canonicalPath = "live_set tracks 0 devices 0";
            confirm.liveIdentity.identityComplete = true;
            confirm.liveIdentity.identityBestEffort = true;
            pipe->pushInbound(acb.pairConfirm(confirm));
            pump(80);
            expect(deliveries.load() >= 1);
            expect(last.type == MessageType::pair_confirm);

            client.notePairConfirm(confirm);
            expect(client.pairing().paired);

            InboxSnapshot snap;
            snap.messageThread = true;
            snap.editorOpen = true;
            snap.linkEnabled = true;
            snap.paired = true;
            snap.runtimeInstanceId = confirm.runtimeInstanceId;
            snap.pairBindingId = confirm.pairBindingId;
            snap.controlRevision = 1;
            snap.projectionBaseEpoch = 1;
            snap.auditionContextEpoch = 1;
            snap.nowMonotonicNs = 1;
            auto req = StageSemanticRequest {};
            req.targetRuntimeInstanceId = snap.runtimeInstanceId;
            req.pairBindingId = snap.pairBindingId;
            req.requestId = randomUuidV4();
            req.expectedControlRevision = 1;
            req.expectedProjectionBaseEpoch = 1;
            req.expectedAuditionContextEpoch = 1;
            req.phrase = "more air";
            req.intensity = 0.4;
            req.expiresAtMonotonicNs = 99;
            req.requestHash = requestHashHex(req);

            ExternalSemanticProposalInbox inbox;
            expect(inbox.evaluateStage(req, snap).disposition == StageDisposition::accept);

            pipe->pushInbound(acb.stage(req));
            pump(80);
            expect(last.type == MessageType::stage_semantic_request);

            client.setEnabled(false);
            for (int i = 0; i < 50 && client.isIoThreadRunning(); ++i)
                pump(10);
            expect(!client.isIoThreadRunning(),
                   "I/O thread must stop after setEnabled(false)");
        }

        beginTest("unsolicited pair_confirm without offer does not pair");
        {
            Client client;
            PairConfirm confirm;
            confirm.pairBindingId = randomUuidV4();
            confirm.runtimeInstanceId = randomUuidV4();
            confirm.humanCode = randomHumanCode();
            client.notePairConfirm(confirm);
            expect(!client.pairing().paired);
            expect(!client.pairing().offerPending);
        }

        beginTest("processor rejects pair_confirm with no PAIR gesture");
        {
            AIEqualizerAudioProcessor proc;
            expect(!proc.getEmberLinkUiState().paired);
            proc.setEmberLinkEnabled(true);

            PairConfirm confirm;
            confirm.pairBindingId = randomUuidV4();
            confirm.runtimeInstanceId = randomUuidV4();
            confirm.humanCode = randomHumanCode();
            WireMessage m;
            m.type = MessageType::pair_confirm;
            m.pairConfirm = confirm;
            proc.injectEmberProposalMessageForTests(std::move(m));
            expect(!proc.getEmberLinkUiState().paired,
                   "pair_confirm without Ember PAIR must not mark paired");
            expect(!proc.getEmberLinkUiState().pairOfferPending);

            proc.requestEmberPairOffer();
            const auto offer = proc.getEmberLinkUiState();
            expect(offer.pairOfferPending);
            expect(!offer.paired);

            PairConfirm mismatch = confirm;
            mismatch.runtimeInstanceId = offer.runtimeInstanceId;
            mismatch.humanCode = offer.humanCode;
            expect(!mismatch.humanCode.empty());
            mismatch.humanCode[0] = (mismatch.humanCode[0] == kHumanAlphabet[0])
                ? kHumanAlphabet[1] : kHumanAlphabet[0];
            WireMessage bad;
            bad.type = MessageType::pair_confirm;
            bad.pairConfirm = mismatch;
            proc.injectEmberProposalMessageForTests(std::move(bad));
            expect(!proc.getEmberLinkUiState().paired,
                   "pair_confirm with mismatched human_code must not pair");
            expect(proc.getEmberLinkUiState().pairOfferPending);

            PairConfirm ok;
            ok.pairBindingId = randomUuidV4();
            ok.runtimeInstanceId = offer.runtimeInstanceId;
            ok.humanCode = offer.humanCode;
            ok.controlRevision = 1;
            ok.confirmedAtMonotonicNs = 2;
            WireMessage good;
            good.type = MessageType::pair_confirm;
            good.pairConfirm = ok;
            proc.injectEmberProposalMessageForTests(std::move(good));
            expect(proc.getEmberLinkUiState().paired);
            expect(!proc.getEmberLinkUiState().pairOfferPending);

            proc.setEmberLinkEnabled(false);
            expect(!proc.isEmberLinkEnabled());
        }

        beginTest("teardown with pending callbacks does not UAF");
        {
            auto encodeConfirm = []() {
                PairConfirm confirm;
                confirm.pairBindingId = randomUuidV4();
                confirm.runtimeInstanceId = randomUuidV4();
                confirm.humanCode = randomHumanCode();
                WireMessage m;
                m.type = MessageType::pair_confirm;
                m.pairConfirm = confirm;
                std::array<std::uint8_t, 32> secret {};
                secret.fill(0x22);
                ProposalSession encoder;
                encoder.setSecret(secret);
                encoder.setAuthenticated(true);
                return std::make_pair(secret, encoder.encode(m, 1, true).frame);
            };

            {
                auto live = std::make_unique<Client>();
                live->setAutoConnectRendezvous(false);
                std::atomic<int> delivered { 0 };
                live->setDelivery([&](WireMessage, EncodedFrame) {
                    delivered.fetch_add(1);
                });
                auto encoded = encodeConfirm();
                live->session().setSecret(encoded.first);
                live->session().setAuthenticated(true);
                juce::WaitableEvent injected;
                std::thread worker([&] {
                    live->injectInboundForTests(encoded.second);
                    injected.signal();
                });
                expect(injected.wait(2000), "live inject must finish");
                worker.join();
                expect(delivered.load() == 0, "callAsync must still be queued");
                pump(80);
                expect(delivered.load() >= 1,
                       "queued inbound must deliver on a live Client");
                live.reset();
            }

            auto client = std::make_unique<Client>();
            client->setAutoConnectRendezvous(false);
            std::atomic<int> late { 0 };
            client->setDelivery([&](WireMessage, EncodedFrame) {
                late.fetch_add(1);
            });
            auto encoded = encodeConfirm();
            client->session().setSecret(encoded.first);
            client->session().setAuthenticated(true);
            juce::WaitableEvent injected;
            std::thread worker([&] {
                client->injectInboundForTests(encoded.second);
                injected.signal();
            });
            expect(injected.wait(2000), "queued inject must finish");
            worker.join();
            expect(late.load() == 0, "delivery must still be queued before teardown");
            const auto genAtTeardown = client->generation();
            client.reset();
            pump(80);
            expect(late.load() == 0,
                   "queued callAsync must not run after Client reset");
            juce::ignoreUnused(genAtTeardown);
        }

        beginTest("no remote APPLY verb is emitted on the wire");
        {
            auto pipe = std::make_shared<InProcessTransport>();
            Client client;
            client.setAutoConnectRendezvous(false);
            client.attachTransport(pipe);
            client.setEnabled(true);
            WireMessage applied;
            applied.type = MessageType::user_applied;
            applied.userApplied.requestId = randomUuidV4();
            applied.userApplied.pairBindingId = randomUuidV4();
            applied.userApplied.targetRuntimeInstanceId = randomUuidV4();
            applied.userApplied.planHash = randomHex64();
            applied.userApplied.audit = makeAudit("user_applied");
            client.session().setAuthenticated(true);
            std::array<std::uint8_t, 32> secret {};
            secret.fill(0x33);
            client.session().setSecret(secret);
            client.sendOutcome(applied);
            const auto out = pipe->takeOutbound();
            expect(!out.empty());
            const std::string body(out.back().body.begin(), out.back().body.end());
            expect(body.find("\"message_type\":\"apply\"") == std::string::npos);
            expect(body.find("set_parameter") == std::string::npos);
            expect(body.find("\"notification_only\":true") != std::string::npos);
            client.setEnabled(false);
        }

        beginTest("offline buffers match with link disabled");
        {
            AIEqualizerAudioProcessor neverOn;
            AIEqualizerAudioProcessor toggled;
            constexpr double sr = 48000.0;
            constexpr int n = 64;
            neverOn.prepareToPlay(sr, n);
            toggled.prepareToPlay(sr, n);

            expect(!toggled.isEmberLinkEnabled());
            toggled.setEmberLinkEnabled(true);
            expect(toggled.isEmberLinkEnabled());
            toggled.setEmberLinkEnabled(false);
            expect(!toggled.isEmberLinkEnabled());
            expect(!toggled.emberProposalHasOpenListener());
            expect(!neverOn.isEmberLinkEnabled());

            juce::AudioBuffer<float> ba(2, n), bb(2, n);
            juce::MidiBuffer midi;
            for (int i = 0; i < n; ++i)
            {
                const float s = 0.1f * std::sin(2.0f * 3.14159265f * 440.0f * (float) i / (float) sr);
                ba.setSample(0, i, s);
                ba.setSample(1, i, s);
                bb.setSample(0, i, s);
                bb.setSample(1, i, s);
            }
            neverOn.processBlock(ba, midi);
            toggled.processBlock(bb, midi);
            float maxDiff = 0.0f;
            for (int i = 0; i < n; ++i)
                maxDiff = std::max(maxDiff, std::abs(ba.getSample(0, i) - bb.getSample(0, i)));
            expect(maxDiff < 1.0e-5f);
            neverOn.releaseResources();
            toggled.releaseResources();
        }

        beginTest("PAIR fails closed when TCP/session is bound to a different rendezvous dir");
        {
            auto pipe = std::make_shared<InProcessTransport>();
            Client client;
            client.setAutoConnectRendezvous(false);
            client.attachTransport(pipe);
            std::array<std::uint8_t, 32> secret {};
            secret.fill(0x44);
            client.session().setSecret(secret);
            client.session().setAuthenticated(true);
            client.setEnabled(true);

            auto dirA = makeControlDir("A");
            auto dirB = makeControlDir("B");
            const auto recordB = writeUsableRendezvous(dirB);
            client.setControlDirectory(dirA);
            client.bindRendezvous(dirB.getChildFile(kRendezvousFileName), recordB);

            PairOffer offer;
            offer.runtimeInstanceId = randomUuidV4();
            offer.humanCode = randomHumanCode();
            offer.controlRevision = 1;
            offer.projectionBaseEpoch = 1;
            offer.auditionContextEpoch = 1;
            offer.expiresAtMonotonicNs = 10'000'000'000ULL;
            offer.editorOpen = true;
            client.sendPairOffer(offer);
            expect(!client.pairing().offerPending,
                   "mismatched rendezvous dirs must not arm local offerPending");
            expect(!takeLastPairOffer(*pipe).has_value(),
                   "mismatched rendezvous dirs must not emit pair_offer");
            dirA.getParentDirectory().deleteRecursively();
            dirB.getParentDirectory().deleteRecursively();
            client.setEnabled(false);
            for (int i = 0; i < 50 && client.isIoThreadRunning(); ++i)
                pump(10);
        }

        beginTest("PAIR fails closed when a bound rendezvous exists but handshake is not done");
        {
            auto pipe = std::make_shared<InProcessTransport>();
            Client client;
            client.setAutoConnectRendezvous(false);
            client.attachTransport(pipe);
            client.setEnabled(true);

            auto dir = makeControlDir("unauth");
            const auto record = writeUsableRendezvous(dir);
            client.setControlDirectory(dir);
            client.bindRendezvous(dir.getChildFile(kRendezvousFileName), record);
            expect(!client.isAuthenticated());

            PairOffer offer;
            offer.runtimeInstanceId = randomUuidV4();
            offer.humanCode = randomHumanCode();
            offer.controlRevision = 1;
            offer.projectionBaseEpoch = 1;
            offer.auditionContextEpoch = 1;
            offer.expiresAtMonotonicNs = 10'000'000'000ULL;
            offer.editorOpen = true;
            client.sendPairOffer(offer);
            expect(!client.pairing().offerPending,
                   "unauthenticated PAIR must not show a local human_code");
            expect(!takeLastPairOffer(*pipe).has_value(),
                   "unauthenticated PAIR must not send pair_offer on the wire");
            dir.getParentDirectory().deleteRecursively();
            client.setEnabled(false);
            for (int i = 0; i < 50 && client.isIoThreadRunning(); ++i)
                pump(10);
        }
    }
};

static EmberProposalLifecycleTest emberProposalLifecycleTest;

class EmberProposalE4IsolationTest : public juce::UnitTest
{
public:
    EmberProposalE4IsolationTest()
        : juce::UnitTest("Ember Proposal E4 Isolation", "Integration") {}

    void runTest() override
    {
        beginTest("message thread ownership");
        expect(juce::MessageManager::existsAndIsCurrentThread());

        beginTest("two processors: PAIR and more-air stage only on A; no inbound APPLY");
        {
            AIEqualizerAudioProcessor procA;
            AIEqualizerAudioProcessor procB;

            auto pipeA = std::make_shared<InProcessTransport>();
            auto pipeB = std::make_shared<InProcessTransport>();
            procA.setEmberLinkEnabled(true);
            procB.setEmberLinkEnabled(true);
            procA.attachEmberProposalTransportForTests(pipeA);
            procB.attachEmberProposalTransportForTests(pipeB);

            SemanticControlPanel panelA(procA.getSemanticEngine());
            SemanticControlPanel panelB(procB.getSemanticEngine());
            panelA.setSampleRate(48000.0);
            panelB.setSampleRate(48000.0);

            int applyA = 0, applyB = 0;
            int legacyA = 0, legacyB = 0;
            panelA.onTextPlanApply = [&](const auto&) {
                ++applyA;
                return SemanticControlPanel::TextApplyFeedback {};
            };
            panelB.onTextPlanApply = [&](const auto&) {
                ++applyB;
                return SemanticControlPanel::TextApplyFeedback {};
            };
            panelA.onEQGenerated = [&](const auto&) { ++legacyA; };
            panelB.onEQGenerated = [&](const auto&) { ++legacyB; };
            panelA.onExternalPlanResult = [&](ReasonCode reason, std::string summary, std::string hash) {
                procA.handleEmberExternalPlanResult(reason, summary, hash);
            };
            panelB.onExternalPlanResult = [&](ReasonCode reason, std::string summary, std::string hash) {
                procB.handleEmberExternalPlanResult(reason, summary, hash);
            };

            int stageCallsA = 0, stageCallsB = 0;
            std::string stagedPhraseA, stagedPhraseB;
            procA.setEmberStageHandler([&](std::string phrase, float intensity) {
                ++stageCallsA;
                stagedPhraseA = phrase;
                panelA.stageExternalCommand(juce::String::fromUTF8(phrase.c_str()), intensity);
            });
            procB.setEmberStageHandler([&](std::string phrase, float intensity) {
                ++stageCallsB;
                stagedPhraseB = phrase;
                panelB.stageExternalCommand(juce::String::fromUTF8(phrase.c_str()), intensity);
            });
            procA.noteEmberEditorOpen(true);
            procB.noteEmberEditorOpen(true);

            const auto beforeA = eqDigest(procA);
            const auto beforeB = eqDigest(procB);
            expect(procA.getEmberLinkUiState().runtimeInstanceId
                       != procB.getEmberLinkUiState().runtimeInstanceId,
                   "two processors must have distinct runtime instance ids");

            expect(!procB.getEmberLinkUiState().pairOfferPending);
            procA.requestEmberPairOffer();
            const auto offerWire = takeLastPairOffer(*pipeA);
            expect(offerWire.has_value(), "PAIR on A must emit pair_offer on the in-process transport");
            const auto offerA = procA.getEmberLinkUiState();
            expect(offerA.pairOfferPending, "PAIR on A must set local offerPending");
            expect(!offerA.paired);
            expect(!procB.getEmberLinkUiState().pairOfferPending,
                   "PAIR gesture on A must not create offerPending on B");
            expect(!procB.getEmberLinkUiState().paired);

            const auto confirm = pairConfirmMatching(offerA);
            expect(!confirm.pairConfirm.humanCode.empty());
            procA.injectEmberProposalMessageForTests(confirm);
            procB.injectEmberProposalMessageForTests(confirm);

            expect(procA.getEmberLinkUiState().paired, "matching pair_confirm pairs only the offerPending instance");
            expect(!procA.getEmberLinkUiState().pairOfferPending);
            expect(!procB.getEmberLinkUiState().paired,
                   "pair_confirm without B offerPending must not pair B");
            expect(!procB.getEmberLinkUiState().pairOfferPending);
            expect(procA.getEmberLinkUiState().pairBindingId == confirm.pairConfirm.pairBindingId);

            if (!offerWire.has_value())
                return;

            ExternalSemanticProposalInbox inboxProbe;
            InboxSnapshot snapA;
            snapA.messageThread = true;
            snapA.editorOpen = true;
            snapA.linkEnabled = true;
            snapA.paired = procA.getEmberLinkUiState().paired;
            snapA.runtimeInstanceId = procA.getEmberLinkUiState().runtimeInstanceId;
            snapA.pairBindingId = procA.getEmberLinkUiState().pairBindingId;
            snapA.controlRevision = offerWire->controlRevision;
            snapA.projectionBaseEpoch = offerWire->projectionBaseEpoch;
            snapA.auditionContextEpoch = offerWire->auditionContextEpoch;
            snapA.nowMonotonicNs = static_cast<std::uint64_t>(
                std::max<std::int64_t>(0, juce::Time::getHighResolutionTicks()));

            const auto stage = stageMoreAir(procA.getEmberLinkUiState(), *offerWire);
            expect(inboxProbe.evaluateStage(stage.stage, snapA).disposition == StageDisposition::accept,
                   "more air must be an inbox accept on A's paired snapshot");

            InboxSnapshot snapB;
            snapB.messageThread = true;
            snapB.editorOpen = true;
            snapB.linkEnabled = true;
            snapB.paired = procB.getEmberLinkUiState().paired;
            snapB.runtimeInstanceId = procB.getEmberLinkUiState().runtimeInstanceId;
            snapB.pairBindingId = procB.getEmberLinkUiState().pairBindingId;
            snapB.controlRevision = offerWire->controlRevision;
            snapB.projectionBaseEpoch = offerWire->projectionBaseEpoch;
            snapB.auditionContextEpoch = offerWire->auditionContextEpoch;
            snapB.nowMonotonicNs = snapA.nowMonotonicNs;
            expect(inboxProbe.evaluateStage(stage.stage, snapB).disposition != StageDisposition::accept,
                   "the same more-air request must not inbox-accept on unpaired B");

            procA.injectEmberProposalMessageForTests(stage);
            procB.injectEmberProposalMessageForTests(stage);

            expect(stageCallsA == 1, "inbox accept on A must reach the Semantic panel stage path");
            expect(stagedPhraseA == "more air");
            expect(procA.getEmberLinkUiState().pendingSource == kPendingSourceLabel);
            expect(panelA.isPendingExternal(), "A's Semantic panel must show the Copilot pending source");
            auto* inputA = dynamic_cast<juce::TextEditor*>(
                panelA.findChildWithID("semanticCommandInput"));
            expect(inputA != nullptr && inputA->getText().trim() == "more air",
                   "A's Semantic panel must stage the more air phrase");
            expect(stageCallsB == 0, "B must not stage from A's stage_semantic_request");
            expect(stagedPhraseB.empty());
            expect(procB.getEmberLinkUiState().pendingSource.empty(),
                   "B must not mark a pending Copilot proposal");
            expect(!panelB.isPendingExternal());
            expect(!procB.getEmberLinkUiState().paired);

            expect(eqDigest(procA) == beforeA,
                   "staging more air on A must not write APVTS / apply Semantic");
            expect(eqDigest(procB) == beforeB,
                   "B APVTS must be untouched");

            pump(50);
            panelA.timerCallback();
            panelB.timerCallback();
            expect(applyA == 0 && applyB == 0,
                   "inbound path must not invoke onTextPlanApply / applySemanticAdjustments");
            expect(legacyA == 0 && legacyB == 0,
                   "inbound path must not fire the legacy onEQGenerated apply");
            expect(eqDigest(procA) == beforeA,
                   "planning after stage must still not write APVTS");
            expect(eqDigest(procB) == beforeB,
                   "B APVTS must stay untouched after the planning tick");

            procA.setEmberStageHandler({});
            procB.setEmberStageHandler({});
            procA.noteEmberEditorOpen(false);
            procB.noteEmberEditorOpen(false);
            procA.setEmberLinkEnabled(false);
            procB.setEmberLinkEnabled(false);
            for (int i = 0; i < 50 && (procA.emberProposalHasOpenListener()
                                       || procB.emberProposalHasOpenListener()); ++i)
                pump(10);
        }
    }
};

static EmberProposalE4IsolationTest emberProposalE4IsolationTest;

class EmberProposalTcpPairTest : public juce::UnitTest
{
public:
    EmberProposalTcpPairTest()
        : juce::UnitTest("Ember Proposal TCP PAIR", "Integration") {}

    void runTest() override
    {
        beginTest("message thread ownership");
        expect(juce::MessageManager::existsAndIsCurrentThread());

        beginTest("TCP handshake then PAIR click emits HMAC pair_offer while IO is polling");
        {
            struct RestoreAutoconnect
            {
                RestoreAutoconnect()
                {
                    unsetenv("EMBER_PROPOSAL_DISABLE_AUTOCONNECT");
                }
                ~RestoreAutoconnect()
                {
                    setenv("EMBER_PROPOSAL_DISABLE_AUTOCONNECT", "1", 1);
                }
            } restoreAutoconnect;
            juce::StreamingSocket listener;
            expect(listener.createListener(0, juce::String(kLoopbackAddress)),
                   "test Observer must listen on loopback");
            const int port = listener.getBoundPort();
            expect(port > 0);

            auto dir = makeControlDir("tcp-pair");
            auto record = writeUsableRendezvous(dir);
            record.listenAddress = kLoopbackAddress;
            record.listenPort = port;
            const auto rdvFile = dir.getChildFile(kRendezvousFileName);
            expect(rdvFile.replaceWithText(serializeRendezvousJson(record)));
            chmod(rdvFile.getFullPathName().toRawUTF8(), 0600);

            FakeAcb acb;
            bool secretOk = false;
            acb.secret = secretFromHex(record.sessionSecretHex, secretOk);
            expect(secretOk);
            acb.session.setSecret(acb.secret);

            AIEqualizerAudioProcessor proc;
            proc.setEmberProposalControlDirectoryForTests(dir);
            proc.noteEmberEditorOpen(true);
            proc.setEmberLinkEnabled(true);

            expect(listener.waitUntilReady(true, 2000) > 0,
                   "Ember must connect after LINK ON + canonical rendezvous");
            std::unique_ptr<juce::StreamingSocket> conn(listener.waitForNextConnection());
            expect(conn != nullptr, "Observer accept must yield a connected socket");
            if (conn == nullptr)
            {
                proc.setEmberLinkEnabled(false);
                dir.getParentDirectory().deleteRecursively();
                return;
            }

            EncodedFrame helloFrame;
            expect(readTcpFrame(*conn, helloFrame, false, 2000), "must receive handshake hello");
            auto ack = acb.ackHello(helloFrame);
            expect(!ack.body.empty());
            expect(writeTcpFrame(*conn, ack), "must write handshake_ack");

            EncodedFrame confirmFrame;
            expect(readTcpFrame(*conn, confirmFrame, false, 2000), "must receive handshake_confirm");
            acb.confirmAndAuth(confirmFrame);
            expect(acb.session.isAuthenticated());

            for (int i = 0; i < 40 && !proc.getEmberLinkUiState().authenticated; ++i)
                pump(25);
            expect(proc.getEmberLinkUiState().authenticated,
                   "editor must publish Connected after handshake_confirm");
            expect(proc.getEmberLinkUiState().statusText == "Connected",
                   "status after handshake must be Connected");

            // Tonight's Live miss: PAIR after Connected, while the client I/O
            // thread is blocked in waitUntilReady(read) holding JUCE readLock.
            pump(150);

            SemanticControlPanel panel(proc.getSemanticEngine());
            panel.setSize(420, 360);
            panel.setEmberLinkUi(proc.getEmberLinkUiState());
            panel.onEmberPairClicked = [&] {
                proc.requestEmberPairOffer();
                panel.setEmberLinkUi(proc.getEmberLinkUiState());
            };
            auto* pairBtn = dynamic_cast<juce::Button*>(panel.findChildWithID("emberPairButton"));
            expect(pairBtn != nullptr, "PAIR control must be emberPairButton");
            expect(pairBtn != nullptr && pairBtn->isEnabled(),
                   "PAIR must be enabled after Connected");
            if (pairBtn != nullptr)
                pairBtn->triggerClick();
            pump(50);
            const auto afterClick = proc.getEmberLinkUiState();
            expect(afterClick.pairOfferPending,
                   "PAIR click must arm local offerPending before the Observer read");
            expect(afterClick.statusText.rfind("PAIR ", 0) == 0,
                   "PAIR click must publish PAIR <code> immediately");

            EncodedFrame offerFrame;
            bool gotOffer = false;
            for (int i = 0; i < 40 && !gotOffer; ++i)
            {
                pump(50);
                gotOffer = readTcpFrame(*conn, offerFrame, true, 50);
            }
            expect(gotOffer,
                   "PAIR after handshake_confirm must write pair_offer on the TCP socket");
            expect(offerFrame.hasHmac,
                   "pair_offer after auth must carry the 32-byte HMAC trailer");
            if (gotOffer)
            {
                auto ingested = acb.session.ingest(offerFrame);
                expect(ingested.ok, "Observer ingest of HMAC pair_offer must succeed");
                expect(ingested.message.type == MessageType::pair_offer,
                       "wire type must be pair_offer so Confirm Pair can arm");
                expect(!ingested.message.pairOffer.humanCode.empty());
                expect(ingested.message.pairOffer.editorOpen);
            }

            const auto ui = proc.getEmberLinkUiState();
            expect(ui.pairOfferPending, "PAIR click must keep local offerPending");
            expect(ui.statusText.rfind("PAIR ", 0) == 0,
                   "editor must show PAIR <human_code>, not stay on Connected");
            expect(!ui.paired);

            conn->close();
            proc.setEmberLinkEnabled(false);
            proc.noteEmberEditorOpen(false);
            for (int i = 0; i < 50 && proc.getEmberLinkUiState().connected; ++i)
                pump(10);
            dir.getParentDirectory().deleteRecursively();
        }
    }
};

static EmberProposalTcpPairTest emberProposalTcpPairTest;
} // namespace
