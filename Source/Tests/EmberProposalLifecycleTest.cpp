#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "../Integration/EmberProposalClient.h"
#include "../Integration/EmberProposalProtocol.h"
#include "../Integration/ExternalSemanticProposalInbox.h"
#include "../PluginProcessor.h"

#include <CoreFoundation/CoreFoundation.h>
#include <atomic>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
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
    }
};

static EmberProposalLifecycleTest emberProposalLifecycleTest;
} // namespace
