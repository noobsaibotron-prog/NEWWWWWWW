#pragma once

#include "EmberProposalProtocol.h"

#include <cstdint>
#include <string>

namespace EmberProposal
{

struct InboxSnapshot
{
    bool messageThread = false;
    bool editorOpen = false;
    bool linkEnabled = false;
    bool paired = false;
    std::string runtimeInstanceId;
    std::string pairBindingId;
    std::uint64_t controlRevision = 0;
    std::uint64_t projectionBaseEpoch = 0;
    std::uint64_t auditionContextEpoch = 0;
    std::uint64_t nowMonotonicNs = 0;
};

enum class StageDisposition
{
    accept,
    target_ui_unavailable,
    unpaired,
    stale_revision,
    expired,
    protocol_error
};

struct StageDecision
{
    StageDisposition disposition = StageDisposition::protocol_error;
    ProtocolErrorCode protocolError = ProtocolErrorCode::malformed_json;
    UnpairedCause unpairedCause = UnpairedCause::ambiguity;
    ReasonCode reason = ReasonCode::protocol_error;
};

class ExternalSemanticProposalInbox
{
public:
    [[nodiscard]] StageDecision evaluateStage(const StageSemanticRequest& request,
                                              const InboxSnapshot& snapshot) const;

    void noteExternalRequest(const std::string& requestId);
    void clearPending();
    [[nodiscard]] const std::string& pendingRequestId() const noexcept { return pendingRequestId_; }
    [[nodiscard]] bool hasPending() const noexcept { return !pendingRequestId_.empty(); }

    void noteLocalInvalidation();
    [[nodiscard]] std::uint64_t localGeneration() const noexcept { return localGeneration_; }

private:
    std::string pendingRequestId_;
    std::uint64_t localGeneration_ = 0;
};

[[nodiscard]] ReasonCode planningStatusToReason(int semanticPlanningStatus) noexcept;

} // namespace EmberProposal
