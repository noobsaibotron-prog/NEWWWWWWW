#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include "../AI/SemanticPlanningService.h"
#include "../Integration/EmberProposalProtocol.h"
#include "../Integration/ExternalSemanticProposalInbox.h"

#include <cmath>
#include <string>

namespace
{
using namespace EmberProposal;

StageSemanticRequest makeRequest(const InboxSnapshot& snap, std::string phrase = "more air")
{
    StageSemanticRequest r;
    r.targetRuntimeInstanceId = snap.runtimeInstanceId;
    r.pairBindingId = snap.pairBindingId;
    r.requestId = randomUuidV4();
    r.expectedControlRevision = snap.controlRevision;
    r.expectedProjectionBaseEpoch = snap.projectionBaseEpoch;
    r.expectedAuditionContextEpoch = snap.auditionContextEpoch;
    r.phrase = std::move(phrase);
    r.intensity = 0.5;
    r.expiresAtMonotonicNs = snap.nowMonotonicNs + 1'000'000'000ULL;
    r.requestHash = requestHashHex(r);
    return r;
}

InboxSnapshot pairedOpen()
{
    InboxSnapshot s;
    s.messageThread = true;
    s.editorOpen = true;
    s.linkEnabled = true;
    s.paired = true;
    s.runtimeInstanceId = randomUuidV4();
    s.pairBindingId = randomUuidV4();
    s.controlRevision = 3;
    s.projectionBaseEpoch = 1;
    s.auditionContextEpoch = 1;
    s.nowMonotonicNs = 100;
    return s;
}

class ExternalSemanticProposalTest : public juce::UnitTest
{
public:
    ExternalSemanticProposalTest()
        : juce::UnitTest("External Semantic Proposal Inbox (E2)", "Integration") {}

    void runTest() override
    {
        beginTest("message thread ownership is required");
        expect(juce::MessageManager::existsAndIsCurrentThread());
        {
            ExternalSemanticProposalInbox inbox;
            auto snap = pairedOpen();
            snap.messageThread = false;
            const auto d = inbox.evaluateStage(makeRequest(snap), snap);
            expect(d.disposition == StageDisposition::protocol_error);
        }

        beginTest("editor closed yields typed target_ui_unavailable and does not accept");
        {
            ExternalSemanticProposalInbox inbox;
            auto snap = pairedOpen();
            snap.editorOpen = false;
            const auto req = makeRequest(snap);
            const auto d = inbox.evaluateStage(req, snap);
            expect(d.disposition == StageDisposition::target_ui_unavailable);
            expect(d.reason == ReasonCode::target_ui_unavailable);
            expect(!inbox.hasPending());
        }

        beginTest("stale control revision is rejected");
        {
            ExternalSemanticProposalInbox inbox;
            auto snap = pairedOpen();
            auto req = makeRequest(snap);
            req.expectedControlRevision = snap.controlRevision + 1;
            req.requestHash = requestHashHex(req);
            const auto d = inbox.evaluateStage(req, snap);
            expect(d.disposition == StageDisposition::stale_revision);
            expect(d.reason == ReasonCode::stale_revision);
        }

        beginTest("stale projection and audition epochs are rejected");
        {
            ExternalSemanticProposalInbox inbox;
            auto snap = pairedOpen();
            auto req = makeRequest(snap);
            req.expectedProjectionBaseEpoch = snap.projectionBaseEpoch + 9;
            req.requestHash = requestHashHex(req);
            expect(inbox.evaluateStage(req, snap).disposition == StageDisposition::stale_revision);

            req = makeRequest(snap);
            req.expectedAuditionContextEpoch = 0;
            req.requestHash = requestHashHex(req);
            expect(inbox.evaluateStage(req, snap).disposition == StageDisposition::stale_revision);
        }

        beginTest("expired request is rejected");
        {
            ExternalSemanticProposalInbox inbox;
            auto snap = pairedOpen();
            auto req = makeRequest(snap);
            req.expiresAtMonotonicNs = snap.nowMonotonicNs;
            req.requestHash = requestHashHex(req);
            expect(inbox.evaluateStage(req, snap).disposition == StageDisposition::expired);
        }

        beginTest("instance mismatch unpairs rather than staging");
        {
            ExternalSemanticProposalInbox inbox;
            auto snap = pairedOpen();
            auto req = makeRequest(snap);
            req.targetRuntimeInstanceId = randomUuidV4();
            req.requestHash = requestHashHex(req);
            const auto d = inbox.evaluateStage(req, snap);
            expect(d.disposition == StageDisposition::unpaired);
            expect(d.unpairedCause == UnpairedCause::ambiguity);
        }

        beginTest("accepted request plus local action invalidates pending");
        {
            ExternalSemanticProposalInbox inbox;
            auto snap = pairedOpen();
            const auto req = makeRequest(snap);
            const auto d = inbox.evaluateStage(req, snap);
            expect(d.disposition == StageDisposition::accept);
            inbox.noteExternalRequest(req.requestId);
            expect(inbox.hasPending());
            inbox.noteLocalInvalidation();
            expect(!inbox.hasPending());
            expect(inbox.localGeneration() > 0);
        }

        beginTest("planning status maps to typed reasons only");
        {
            using S = AIEQPerceptual::SemanticPlanningStatus;
            expect(planningStatusToReason(static_cast<int>(S::Ready)) == ReasonCode::plan_staged);
            expect(planningStatusToReason(static_cast<int>(S::UnknownIntent)) == ReasonCode::unknown_intent);
            expect(planningStatusToReason(static_cast<int>(S::ContradictoryIntent))
                   == ReasonCode::contradictory_intent);
            expect(planningStatusToReason(static_cast<int>(S::NoSafeMove)) == ReasonCode::no_safe_move);
            expect(planningStatusToReason(static_cast<int>(S::InternalError)) == ReasonCode::internal_error);
            expect(planningStatusToReason(static_cast<int>(S::Cancelled)) == ReasonCode::stale_revision);
        }

        beginTest("latest-request-wins: local submit drops in-flight external generation");
        {
            AIEQPerceptual::SemanticPlanningService svc;
            svc.start();
            const auto externalGen = svc.submit("more air", 0.5f, 48000.0);
            const auto localGen = svc.submit("warmer", 1.0f, 48000.0);
            expect(localGen != externalGen);
            expect(svc.waitUntilQuiescent(8000));
            const auto result = svc.takeCurrentResult();
            expect(result.has_value());
            if (result.has_value())
                expect(result->generation == localGen);
            svc.stop();
        }

        beginTest("link disabled is unpaired not silent stage");
        {
            ExternalSemanticProposalInbox inbox;
            auto snap = pairedOpen();
            snap.linkEnabled = false;
            const auto d = inbox.evaluateStage(makeRequest(snap), snap);
            expect(d.disposition == StageDisposition::unpaired);
            expect(d.unpairedCause == UnpairedCause::ember_link_disabled);
        }

        beginTest("requestHash mismatch is protocol_error");
        {
            ExternalSemanticProposalInbox inbox;
            auto snap = pairedOpen();
            auto req = makeRequest(snap);
            expect(req.requestHash.size() == 64);
            req.requestHash[0] = req.requestHash[0] == 'a' ? 'b' : 'a';
            const auto d = inbox.evaluateStage(req, snap);
            expect(d.disposition == StageDisposition::protocol_error);
            expect(d.reason == ReasonCode::protocol_error);
            expect(!inbox.hasPending());
        }

        beginTest("unpaired snapshot with matching ids still unpairs");
        {
            ExternalSemanticProposalInbox inbox;
            auto snap = pairedOpen();
            snap.paired = false;
            const auto d = inbox.evaluateStage(makeRequest(snap), snap);
            expect(d.disposition == StageDisposition::unpaired);
            expect(d.reason == ReasonCode::unpaired);
            expect(d.unpairedCause == UnpairedCause::user_unpair);
            expect(!inbox.hasPending());
        }

        beginTest("empty pair or instance ids take the unpaired path");
        {
            ExternalSemanticProposalInbox inbox;
            auto snap = pairedOpen();
            snap.pairBindingId.clear();
            auto d = inbox.evaluateStage(makeRequest(snap), snap);
            expect(d.disposition == StageDisposition::unpaired);
            expect(d.unpairedCause == UnpairedCause::user_unpair);

            snap = pairedOpen();
            snap.runtimeInstanceId.clear();
            d = inbox.evaluateStage(makeRequest(snap), snap);
            expect(d.disposition == StageDisposition::unpaired);
            expect(d.unpairedCause == UnpairedCause::user_unpair);
        }
    }
};

static ExternalSemanticProposalTest externalSemanticProposalTest;
} // namespace
