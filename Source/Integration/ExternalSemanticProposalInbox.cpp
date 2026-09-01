#include "ExternalSemanticProposalInbox.h"

#include "../AI/SemanticPlanningService.h"

namespace EmberProposal
{

StageDecision ExternalSemanticProposalInbox::evaluateStage(const StageSemanticRequest& request,
                                                           const InboxSnapshot& snapshot) const
{
    StageDecision decision;
    if (!snapshot.messageThread)
    {
        decision.disposition = StageDisposition::protocol_error;
        decision.protocolError = ProtocolErrorCode::unauthenticated;
        decision.reason = ReasonCode::protocol_error;
        return decision;
    }

    if (!snapshot.linkEnabled)
    {
        decision.disposition = StageDisposition::unpaired;
        decision.unpairedCause = UnpairedCause::ember_link_disabled;
        decision.reason = ReasonCode::unpaired;
        return decision;
    }

    if (!snapshot.paired || snapshot.pairBindingId.empty() || snapshot.runtimeInstanceId.empty())
    {
        decision.disposition = StageDisposition::unpaired;
        decision.unpairedCause = UnpairedCause::user_unpair;
        decision.reason = ReasonCode::unpaired;
        return decision;
    }

    if (request.targetRuntimeInstanceId != snapshot.runtimeInstanceId
        || request.pairBindingId != snapshot.pairBindingId)
    {
        decision.disposition = StageDisposition::unpaired;
        decision.unpairedCause = UnpairedCause::ambiguity;
        decision.reason = ReasonCode::unpaired;
        return decision;
    }

    if (request.expiresAtMonotonicNs <= snapshot.nowMonotonicNs)
    {
        decision.disposition = StageDisposition::expired;
        decision.reason = ReasonCode::expired;
        return decision;
    }

    if (request.expectedControlRevision != snapshot.controlRevision
        || request.expectedProjectionBaseEpoch != snapshot.projectionBaseEpoch
        || request.expectedAuditionContextEpoch != snapshot.auditionContextEpoch)
    {
        decision.disposition = StageDisposition::stale_revision;
        decision.reason = ReasonCode::stale_revision;
        return decision;
    }

    if (!snapshot.editorOpen)
    {
        decision.disposition = StageDisposition::target_ui_unavailable;
        decision.reason = ReasonCode::target_ui_unavailable;
        return decision;
    }

    if (!constantTimeHexEquals(requestHashHex(request), request.requestHash))
    {
        decision.disposition = StageDisposition::protocol_error;
        decision.protocolError = ProtocolErrorCode::missing_required;
        decision.reason = ReasonCode::protocol_error;
        return decision;
    }

    decision.disposition = StageDisposition::accept;
    decision.reason = ReasonCode::plan_staged;
    return decision;
}

void ExternalSemanticProposalInbox::noteExternalRequest(const std::string& requestId)
{
    pendingRequestId_ = requestId;
}

void ExternalSemanticProposalInbox::clearPending()
{
    pendingRequestId_.clear();
}

void ExternalSemanticProposalInbox::noteLocalInvalidation()
{
    ++localGeneration_;
    pendingRequestId_.clear();
}

ReasonCode planningStatusToReason(int semanticPlanningStatus) noexcept
{
    using Status = AIEQPerceptual::SemanticPlanningStatus;
    switch (static_cast<Status>(semanticPlanningStatus))
    {
        case Status::Ready:               return ReasonCode::plan_staged;
        case Status::UnknownIntent:       return ReasonCode::unknown_intent;
        case Status::ContradictoryIntent: return ReasonCode::contradictory_intent;
        case Status::NoSafeMove:          return ReasonCode::no_safe_move;
        case Status::InternalError:       return ReasonCode::internal_error;
        case Status::Cancelled:           return ReasonCode::stale_revision;
    }
    return ReasonCode::internal_error;
}

} // namespace EmberProposal
