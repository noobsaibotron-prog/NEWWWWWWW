#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include "../Integration/EmberProposalProtocol.h"
#include "../Integration/EmberProposalClient.h"

#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace
{
using namespace EmberProposal;

std::string uuid() { return randomUuidV4(); }

WireMessage helloMessage()
{
    WireMessage m;
    m.type = MessageType::handshake;
    m.handshakePhase = HandshakePhase::hello;
    m.hello.clientNonceHex = randomHex64();
    return m;
}

class EmberProposalProtocolTest : public juce::UnitTest
{
public:
    EmberProposalProtocolTest()
        : juce::UnitTest("Ember Proposal Protocol (E2)", "Integration") {}

    void runTest() override
    {
        beginTest("message thread ownership");
        expect(juce::MessageManager::existsAndIsCurrentThread(),
               "protocol tests run on the JUCE message thread");

        beginTest("catalog has no APPLY or set_parameter verbs");
        expect(isForbiddenMessageType("apply"));
        expect(isForbiddenMessageType("set_parameter"));
        expect(isForbiddenMessageType("remote_apply"));
        expect(isForbiddenMessageType("cmd"));
        expect(!messageTypeFromString("apply").has_value());
        expect(messageTypeFromString("stage_semantic_request").has_value());
        expect(messageTypeFromString("user_applied").has_value());

        beginTest("valid stage_semantic_request round-trips");
        {
            WireMessage m;
            m.type = MessageType::stage_semantic_request;
            m.stage.targetRuntimeInstanceId = uuid();
            m.stage.pairBindingId = uuid();
            m.stage.requestId = uuid();
            m.stage.expectedControlRevision = 3;
            m.stage.expectedProjectionBaseEpoch = 1;
            m.stage.expectedAuditionContextEpoch = 2;
            m.stage.phrase = "more air";
            m.stage.intensity = 0.5;
            m.stage.expiresAtMonotonicNs = 5'000'000'000ULL;
            m.stage.requestHash = requestHashHex(m.stage);
            const auto json = serializeWireJson(m);
            const auto parsed = parseWireJson(json);
            expect(parsed.ok, toString(parsed.error));
            expect(parsed.message.type == MessageType::stage_semantic_request);
            expect(parsed.message.stage.phrase == "more air");
            expect(std::abs(parsed.message.stage.intensity - 0.5) < 1.0e-12);
            expect(parsed.message.stage.requestHash == m.stage.requestHash);
        }

        beginTest("forbidden apply verb is protocol_error forbidden_verb");
        {
            const std::string json =
                "{\"record_kind\":\"wire_message\",\"protocol\":\"ember.proposal.v1\","
                "\"protocol_major\":1,\"protocol_minor\":0,\"message_type\":\"apply\","
                "\"seq\":0,\"sent_at_monotonic_ns\":0,\"payload\":{}}";
            const auto parsed = parseWireJson(json);
            expect(!parsed.ok);
            expect(parsed.error == ProtocolErrorCode::forbidden_verb);
        }

        beginTest("duplicate JSON keys are rejected");
        {
            const std::string json =
                "{\"record_kind\":\"wire_message\",\"record_kind\":\"wire_message\","
                "\"protocol\":\"ember.proposal.v1\",\"protocol_major\":1,\"protocol_minor\":0,"
                "\"message_type\":\"handshake\",\"seq\":0,\"sent_at_monotonic_ns\":0,"
                "\"payload\":{\"phase\":\"hello\",\"client_nonce_hex\":\""
                + randomHex64() + "\",\"protocol_major\":1,\"protocol_minor\":0,"
                "\"capabilities\":[\"proposal\"]}}";
            const auto parsed = parseWireJson(json);
            expect(!parsed.ok);
            expect(parsed.error == ProtocolErrorCode::duplicate_key);
        }

        beginTest("unknown keys are rejected");
        {
            WireMessage m = helloMessage();
            auto json = serializeWireJson(m);
            const auto insertAt = json.rfind('}');
            json.insert(insertAt, ",\"extra\":true");
            const auto parsed = parseWireJson(json);
            expect(!parsed.ok);
            expect(parsed.error == ProtocolErrorCode::unknown_key);
        }

        beginTest("non-finite intensity is rejected");
        {
            WireMessage m;
            m.type = MessageType::stage_semantic_request;
            m.stage.targetRuntimeInstanceId = uuid();
            m.stage.pairBindingId = uuid();
            m.stage.requestId = uuid();
            m.stage.phrase = "x";
            m.stage.intensity = 0.5;
            m.stage.expiresAtMonotonicNs = 1;
            m.stage.requestHash = std::string(64, 'a');
            auto json = serializeWireJson(m);
            const auto needle = std::string("\"intensity\":0.5");
            const auto pos = json.find(needle);
            expect(pos != std::string::npos);
            json.replace(pos, needle.size(), "\"intensity\":NaN");
            const auto parsed = parseWireJson(json);
            expect(!parsed.ok);
            expect(parsed.error == ProtocolErrorCode::non_finite_number
                   || parsed.error == ProtocolErrorCode::malformed_json);
        }

        beginTest("oversized frame is rejected");
        {
            EncodedFrame frame;
            frame.body.assign(kMaxFrameBytes + 1, 'x');
            ProposalSession session;
            const auto ingested = session.ingest(frame);
            expect(!ingested.ok);
            expect(ingested.error == ProtocolErrorCode::oversized_frame);
        }

        beginTest("nesting overflow is rejected");
        {
            std::string json = "{\"payload\":";
            for (int i = 0; i < 20; ++i)
                json += "{\"k\":";
            json += "1";
            for (int i = 0; i < 20; ++i)
                json += "}";
            json += "}";
            const auto parsed = parseWireJson(json);
            expect(!parsed.ok);
            expect(parsed.error == ProtocolErrorCode::nesting_exceeded
                   || parsed.error == ProtocolErrorCode::missing_required
                   || parsed.error == ProtocolErrorCode::malformed_json);
        }

        beginTest("incompatible major is rejected");
        {
            auto json = serializeWireJson(helloMessage());
            const auto pos = json.find("\"protocol_major\":1");
            expect(pos != std::string::npos);
            json.replace(pos, std::strlen("\"protocol_major\":1"), "\"protocol_major\":2");
            const auto parsed = parseWireJson(json);
            expect(!parsed.ok);
            expect(parsed.error == ProtocolErrorCode::incompatible_major);
        }

        beginTest("unknown write capability is rejected");
        {
            WireMessage m = helloMessage();
            m.hello.capabilities = { "proposal", "apply" };
            const auto parsed = parseWireJson(serializeWireJson(m));
            expect(!parsed.ok);
            expect(parsed.error == ProtocolErrorCode::unknown_write_capability
                   || parsed.error == ProtocolErrorCode::missing_required);
        }

        beginTest("seq replay reorder and gap");
        {
            ProposalSession session;
            auto first = session.encode(helloMessage(), 1, false);
            expect(first.ok);
            auto ok1 = session.ingest(first.frame);
            expect(ok1.ok);

            auto replay = session.ingest(first.frame);
            expect(!replay.ok);
            expect(replay.error == ProtocolErrorCode::replay);

            ProposalSession gapped;
            WireMessage second = helloMessage();
            second.seq = 2;
            const auto body = serializeWireJson(second);
            EncodedFrame frame;
            frame.body.assign(body.begin(), body.end());
            auto gap = gapped.ingest(frame);
            expect(!gap.ok);
            expect(gap.error == ProtocolErrorCode::seq_gap);

            ProposalSession reordered;
            WireMessage high = helloMessage();
            high.seq = 1;
            auto highBody = serializeWireJson(high);
            EncodedFrame highFrame;
            highFrame.body.assign(highBody.begin(), highBody.end());
            expect(!reordered.ingest(highFrame).ok);
        }

        beginTest("HMAC mismatch after auth closes");
        {
            ProposalSession session;
            std::array<std::uint8_t, 32> secret {};
            secret.fill(0x11);
            session.setSecret(secret);
            session.setAuthenticated(true);
            auto encoded = session.encode(helloMessage(), 1, true);
            expect(encoded.ok);
            encoded.frame.hmac[0] ^= 0xff;
            const auto ingested = session.ingest(encoded.frame);
            expect(!ingested.ok);
            expect(ingested.error == ProtocolErrorCode::hmac_mismatch);
        }

        beginTest("user_applied requires notification_only true");
        {
            WireMessage m;
            m.type = MessageType::user_applied;
            m.userApplied.requestId = uuid();
            m.userApplied.pairBindingId = uuid();
            m.userApplied.targetRuntimeInstanceId = uuid();
            m.userApplied.planHash = randomHex64();
            m.userApplied.audit = makeAudit("user_applied");
            auto json = serializeWireJson(m);
            expect(json.find("\"notification_only\":true") != std::string::npos);
            auto parsed = parseWireJson(json);
            expect(parsed.ok, toString(parsed.error));

            const auto pos = json.find("\"notification_only\":true");
            json.replace(pos, std::strlen("\"notification_only\":true"),
                         "\"notification_only\":false");
            parsed = parseWireJson(json);
            expect(!parsed.ok);
            expect(parsed.error == ProtocolErrorCode::missing_required);
        }

        beginTest("rendezvous rejects non-loopback and dev-token");
        {
            RendezvousRecord record;
            record.listenAddress = "0.0.0.0";
            record.listenPort = 9;
            record.sessionUuid = uuid();
            record.sessionSecretHex = randomHex64();
            record.expiresAtUnixS = 2'000'000'000;
            record.createdAtUnixS = 1;
            ProtocolErrorCode error = ProtocolErrorCode::malformed_json;
            expect(!validateRendezvous(record, 10, error));
            expect(error == ProtocolErrorCode::stale_rendezvous);

            record.listenAddress = kLoopbackAddress;
            record.sessionSecretHex = kForbiddenSecret;
            expect(!validateRendezvous(record, 10, error));
            expect(error == ProtocolErrorCode::expired_secret);
        }

        beginTest("phrase longer than 256 bytes is rejected");
        {
            WireMessage m;
            m.type = MessageType::stage_semantic_request;
            m.stage.targetRuntimeInstanceId = uuid();
            m.stage.pairBindingId = uuid();
            m.stage.requestId = uuid();
            m.stage.phrase = std::string(257, 'a');
            m.stage.intensity = 0.1;
            m.stage.expiresAtMonotonicNs = 10;
            m.stage.requestHash = std::string(64, 'b');
            const auto parsed = parseWireJson(serializeWireJson(m));
            expect(!parsed.ok);
            expect(parsed.error == ProtocolErrorCode::oversized_frame);
        }

        beginTest("canonical rendezvous is Application Support AbletonCopilotBridge control");
        {
            const auto path = defaultRendezvousFile().getFullPathName().replaceCharacter('\\', '/');
            expect(path.contains(kCanonicalControlRelativePath), path.toStdString());
            expect(path.fromLastOccurrenceOf("/", false, false) == kRendezvousFileName);
            expect(!path.contains("/Library/AbletonCopilotBridge/"),
                   "JUCE userApplicationDataDirectory must not be used without Application Support");
        }

        beginTest("rendezvous file that is not 0600 is fail-closed");
        {
            const auto root = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                  .getChildFile("ember-rdv-perm")
                                  .getChildFile(juce::Uuid().toDashedString());
            const auto control = root.getChildFile("control");
            expect(control.createDirectory());
            chmod(control.getFullPathName().toRawUTF8(), 0700);

            RendezvousRecord record;
            record.listenAddress = kLoopbackAddress;
            record.listenPort = 23456;
            record.sessionUuid = uuid();
            record.sessionSecretHex = randomHex64();
            record.serverEpoch = 1;
            record.expiresAtUnixS = 2'000'000'000;
            record.createdAtUnixS = 1;
            const auto file = control.getChildFile(kRendezvousFileName);
            expect(file.replaceWithText(serializeRendezvousJson(record)));
            chmod(file.getFullPathName().toRawUTF8(), 0644);

            ProtocolErrorCode error = ProtocolErrorCode::malformed_json;
            expect(!readRendezvousFile(file, 10, error));
            expect(error == ProtocolErrorCode::stale_rendezvous);

            chmod(file.getFullPathName().toRawUTF8(), 0600);
            error = ProtocolErrorCode::malformed_json;
            const auto ok = readRendezvousFile(file, 10, error);
            expect(ok.has_value(), toString(error));
            root.deleteRecursively();
        }
    }
};

static EmberProposalProtocolTest emberProposalProtocolTest;
} // namespace
