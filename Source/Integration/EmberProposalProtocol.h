#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace EmberProposal
{

inline constexpr const char* kProtocolName = "ember.proposal.v1";
inline constexpr int kProtocolMajor = 1;
inline constexpr int kProtocolMinor = 0;
inline constexpr int kMaxFrameBytes = 65536;
inline constexpr int kMaxNesting = 16;
inline constexpr int kPhraseMaxBytes = 256;
inline constexpr int kSummaryMaxChars = 128;
inline constexpr int kHex64Len = 64;
inline constexpr int kHumanCodeLen = 6;
inline constexpr const char* kHumanAlphabet = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
inline constexpr const char* kForbiddenSecret = "dev-token";
inline constexpr const char* kLoopbackAddress = "127.0.0.1";
inline constexpr const char* kPendingSourceLabel = "Ableton Copilot";

enum class MessageType
{
    handshake,
    pair_offer,
    pair_confirm,
    unpair,
    stage_semantic_request,
    plan_staged,
    unknown_intent,
    contradictory_intent,
    no_safe_move,
    internal_error,
    stale_revision,
    target_ui_unavailable,
    unpaired,
    expired,
    protocol_error,
    user_applied,
    user_rejected
};

enum class ReasonCode
{
    plan_staged,
    unknown_intent,
    contradictory_intent,
    no_safe_move,
    internal_error,
    stale_revision,
    target_ui_unavailable,
    unpaired,
    expired,
    protocol_error,
    user_applied,
    user_rejected
};

enum class ProtocolErrorCode
{
    incompatible_major,
    unknown_write_capability,
    replay,
    reorder,
    oversized_frame,
    duplicate_key,
    non_finite_number,
    unknown_key,
    invalid_utf8,
    hmac_mismatch,
    nonce_reuse,
    nesting_exceeded,
    malformed_json,
    missing_required,
    unauthenticated,
    expired_secret,
    stale_rendezvous,
    forbidden_verb,
    seq_gap
};

enum class UnpairedCause
{
    user_unpair,
    duplicate_instance,
    duplicate_human_code,
    stale_live_identity,
    disconnect,
    reload,
    delete_recreate,
    epoch_change,
    ambiguity,
    kill_switch,
    ember_link_disabled,
    editor_closed
};

enum class HandshakePhase { hello, ack, confirm };

struct EncodedFrame
{
    std::vector<std::uint8_t> body;
    std::array<std::uint8_t, 32> hmac {};
    bool hasHmac = false;
};

struct AuditMetadata
{
    int byteCount = 0;
    std::string sha256Hex;
    std::optional<int> latencyMs;
};

struct LiveIdentity
{
    std::optional<std::string> trackId;
    std::optional<std::string> deviceId;
    std::optional<std::string> canonicalPath;
    bool identityComplete = false;
    bool identityBestEffort = false;
};

struct HandshakeHello
{
    std::string clientNonceHex;
    int protocolMajor = kProtocolMajor;
    int protocolMinor = kProtocolMinor;
    std::vector<std::string> capabilities { "proposal" };
};

struct HandshakeAck
{
    std::string clientNonceHex;
    std::string serverNonceHex;
    std::string hmacSha256Hex;
    std::string sessionUuid;
    std::uint64_t serverEpoch = 0;
    int protocolMajor = kProtocolMajor;
    int protocolMinor = kProtocolMinor;
};

struct HandshakeConfirm
{
    std::string clientNonceHex;
    std::string serverNonceHex;
    std::string hmacSha256Hex;
};

struct PairOffer
{
    std::string runtimeInstanceId;
    std::string humanCode;
    std::uint64_t controlRevision = 0;
    std::uint64_t projectionBaseEpoch = 0;
    std::uint64_t auditionContextEpoch = 0;
    std::uint64_t expiresAtMonotonicNs = 0;
    bool editorOpen = false;
};

struct PairConfirm
{
    std::string pairBindingId;
    std::string runtimeInstanceId;
    std::string humanCode;
    LiveIdentity liveIdentity;
    std::uint64_t controlRevision = 0;
    std::uint64_t confirmedAtMonotonicNs = 0;
};

struct UnpairCommand
{
    std::string pairBindingId;
    std::string runtimeInstanceId;
    UnpairedCause unpairedCause = UnpairedCause::user_unpair;
};

struct StageSemanticRequest
{
    std::string targetRuntimeInstanceId;
    std::string pairBindingId;
    std::string requestId;
    std::uint64_t expectedControlRevision = 0;
    std::uint64_t expectedProjectionBaseEpoch = 0;
    std::uint64_t expectedAuditionContextEpoch = 0;
    std::string phrase;
    double intensity = 0.0;
    std::uint64_t expiresAtMonotonicNs = 0;
    std::string requestHash;
};

struct OutcomeBase
{
    std::string requestId;
    std::string pairBindingId;
    std::string targetRuntimeInstanceId;
    std::uint64_t controlRevision = 0;
    std::uint64_t projectionBaseEpoch = 0;
    std::uint64_t auditionContextEpoch = 0;
    AuditMetadata audit;
};

struct PlanStaged : OutcomeBase
{
    std::string planHash;
    std::string summary;
};

struct UnpairedOutcome : OutcomeBase
{
    UnpairedCause unpairedCause = UnpairedCause::disconnect;
};

struct ProtocolError
{
    std::optional<std::string> requestId;
    std::optional<std::string> pairBindingId;
    std::optional<std::string> targetRuntimeInstanceId;
    ProtocolErrorCode protocolErrorCode = ProtocolErrorCode::malformed_json;
    bool closeConnection = true;
    AuditMetadata audit;
};

struct UserApplied : OutcomeBase
{
    std::string planHash;
};

struct UserRejected : OutcomeBase {};

struct WireMessage
{
    MessageType type = MessageType::protocol_error;
    std::uint64_t seq = 0;
    std::int64_t sentAtMonotonicNs = 0;
    HandshakePhase handshakePhase = HandshakePhase::hello;
    HandshakeHello hello;
    HandshakeAck ack;
    HandshakeConfirm confirm;
    PairOffer pairOffer;
    PairConfirm pairConfirm;
    UnpairCommand unpair;
    StageSemanticRequest stage;
    PlanStaged planStaged;
    OutcomeBase refusal;
    UnpairedOutcome unpaired;
    ProtocolError protocolError;
    UserApplied userApplied;
    UserRejected userRejected;
};

struct RendezvousRecord
{
    std::string listenAddress;
    int listenPort = 0;
    std::string sessionUuid;
    std::string sessionSecretHex;
    std::uint64_t serverEpoch = 0;
    std::int64_t expiresAtUnixS = 0;
    std::int64_t createdAtUnixS = 0;
};

struct IngestResult
{
    bool ok = false;
    ProtocolErrorCode error = ProtocolErrorCode::malformed_json;
    WireMessage message;
};

struct EncodeResult
{
    bool ok = false;
    EncodedFrame frame;
    WireMessage message;
};

struct LinkUiState
{
    bool linkEnabled = false;
    bool connected = false;
    bool authenticated = false;
    bool paired = false;
    bool pairOfferPending = false;
    bool hasOpenListener = false;
    std::string humanCode;
    std::string runtimeInstanceId;
    std::string pairBindingId;
    std::string pendingSource;
    std::string statusText { "Link off" };
};

[[nodiscard]] const char* toString(MessageType type) noexcept;
[[nodiscard]] const char* toString(ReasonCode code) noexcept;
[[nodiscard]] const char* toString(ProtocolErrorCode code) noexcept;
[[nodiscard]] const char* toString(UnpairedCause cause) noexcept;
[[nodiscard]] std::optional<MessageType> messageTypeFromString(std::string_view text);
[[nodiscard]] std::optional<ProtocolErrorCode> protocolErrorFromString(std::string_view text);
[[nodiscard]] std::optional<UnpairedCause> unpairedCauseFromString(std::string_view text);
[[nodiscard]] ReasonCode reasonFromMessageType(MessageType type) noexcept;
[[nodiscard]] MessageType messageTypeFromReason(ReasonCode code) noexcept;
[[nodiscard]] bool isForbiddenMessageType(std::string_view text) noexcept;
[[nodiscard]] bool isOutcomeMessage(MessageType type) noexcept;

[[nodiscard]] bool isHex64(std::string_view text) noexcept;
[[nodiscard]] bool isUuidV4(std::string_view text) noexcept;
[[nodiscard]] bool isHumanCode(std::string_view text) noexcept;
[[nodiscard]] bool isValidUtf8(std::string_view text) noexcept;

[[nodiscard]] std::string toHex(const std::uint8_t* data, std::size_t n);
[[nodiscard]] bool fromHex(std::string_view hex, std::uint8_t* out, std::size_t n);
void sha256(const std::uint8_t* data, std::size_t n, std::uint8_t out[32]);
void hmacSha256(const std::uint8_t* key, std::size_t keyLen,
                const std::uint8_t* data, std::size_t dataLen,
                std::uint8_t out[32]);
[[nodiscard]] bool constantTimeEquals(const std::uint8_t* a, const std::uint8_t* b, std::size_t n) noexcept;
[[nodiscard]] bool constantTimeHexEquals(std::string_view a, std::string_view b) noexcept;

void fillCsprng(std::uint8_t* out, std::size_t n);
[[nodiscard]] std::string randomHex64();
[[nodiscard]] std::string randomUuidV4();
[[nodiscard]] std::string randomHumanCode();
[[nodiscard]] std::array<std::uint8_t, 32> secretFromHex(std::string_view hex, bool& ok);

[[nodiscard]] std::string canonicalRequestJson(const StageSemanticRequest& request);
[[nodiscard]] std::string requestHashHex(const StageSemanticRequest& request);
[[nodiscard]] AuditMetadata makeAudit(const std::uint8_t* data, std::size_t n, std::optional<int> latencyMs = 0);
[[nodiscard]] AuditMetadata makeAudit(std::string_view text, std::optional<int> latencyMs = 0);

[[nodiscard]] std::string serializeWireJson(const WireMessage& message);
[[nodiscard]] std::string serializeRendezvousJson(const RendezvousRecord& record);
[[nodiscard]] IngestResult parseWireJson(std::string_view json);
[[nodiscard]] std::optional<RendezvousRecord> parseRendezvousJson(std::string_view json, ProtocolErrorCode& error);

class ProposalSession
{
public:
    void reset();
    void setSecret(const std::array<std::uint8_t, 32>& secret);
    void clearSecret();
    [[nodiscard]] bool hasSecret() const noexcept { return hasSecretFlag; }
    void setAuthenticated(bool value) noexcept { authenticated = value; }
    [[nodiscard]] bool isAuthenticated() const noexcept { return authenticated; }

    [[nodiscard]] IngestResult ingest(const EncodedFrame& frame);
    [[nodiscard]] EncodeResult encode(const WireMessage& message, std::int64_t nowNs, bool macOverride);
    bool applyClientAck(const HandshakeAck& ack, HandshakeConfirm& confirm);

    [[nodiscard]] std::uint64_t nextOutboundSeq() const noexcept { return outboundSeq; }

    std::string lastClientNonceHex;
    std::array<std::uint8_t, 32> clientNonce {};
    std::array<std::uint8_t, 32> serverNonce {};
    bool hasClientNonce = false;
    bool hasServerNonce = false;

private:
    std::array<std::uint8_t, 32> secret {};
    bool hasSecretFlag = false;
    bool authenticated = false;
    std::uint64_t expectedSeq = 0;
    std::optional<std::uint64_t> lastSeq;
    std::uint64_t outboundSeq = 0;
    std::vector<std::string> seenNonces;
};

[[nodiscard]] EncodedFrame frameFromBody(std::string_view body, const std::uint8_t* hmac);
[[nodiscard]] bool validateRendezvous(const RendezvousRecord& record, std::int64_t nowUnixS, ProtocolErrorCode& error);

} // namespace EmberProposal
