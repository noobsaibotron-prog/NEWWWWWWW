#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include "../GUI/ProblemRowDisclosure.h"

class ProblemRowDisclosureTest final : public juce::UnitTest
{
public:
    ProblemRowDisclosureTest()
        : juce::UnitTest ("Problem row progressive disclosure", "AI-Diag")
    {}

    void runTest() override
    {
        using EmberUI::ProblemRowDisclosure;
        using EmberUI::ProblemRowView;

        beginTest ("default is fully collapsed");
        {
            ProblemRowDisclosure d;
            expectEquals ((int) d.viewFor (0), (int) ProblemRowView::Collapsed);
            expect (! d.hasExpansion());
            expectEquals (d.selectedIndex, -1);
            expectEquals (d.expandedIndex, -1);
        }

        beginTest ("select marks only that row");
        {
            ProblemRowDisclosure d;
            d.select (1, 3);
            expectEquals ((int) d.viewFor (0), (int) ProblemRowView::Collapsed);
            expectEquals ((int) d.viewFor (1), (int) ProblemRowView::Selected);
            expectEquals ((int) d.viewFor (2), (int) ProblemRowView::Collapsed);
            expect (! d.hasExpansion());
        }

        beginTest ("out-of-range select clears state");
        {
            ProblemRowDisclosure d;
            d.select (0, 2);
            d.select (5, 2);
            expectEquals (d.selectedIndex, -1);
            expect (! d.hasExpansion());
            d.select (0, 0);
            expectEquals (d.selectedIndex, -1);
        }

        beginTest ("toggle expand selects and expands the same row");
        {
            ProblemRowDisclosure d;
            d.toggleExpand (2, 4);
            expectEquals (d.selectedIndex, 2);
            expectEquals (d.expandedIndex, 2);
            expectEquals ((int) d.viewFor (2), (int) ProblemRowView::Expanded);
            expect (d.hasExpansion());
        }

        beginTest ("second toggle collapses expansion but keeps selection");
        {
            ProblemRowDisclosure d;
            d.toggleExpand (0, 2);
            d.toggleExpand (0, 2);
            expectEquals (d.selectedIndex, 0);
            expectEquals (d.expandedIndex, -1);
            expectEquals ((int) d.viewFor (0), (int) ProblemRowView::Selected);
        }

        beginTest ("at most one row is expanded");
        {
            ProblemRowDisclosure d;
            d.toggleExpand (0, 3);
            d.toggleExpand (2, 3);
            expectEquals (d.selectedIndex, 2);
            expectEquals (d.expandedIndex, 2);
            expectEquals ((int) d.viewFor (0), (int) ProblemRowView::Collapsed);
            expectEquals ((int) d.viewFor (2), (int) ProblemRowView::Expanded);
        }

        beginTest ("selecting another row collapses expansion");
        {
            ProblemRowDisclosure d;
            d.toggleExpand (0, 3);
            d.select (1, 3);
            expectEquals (d.selectedIndex, 1);
            expectEquals (d.expandedIndex, -1);
            expectEquals ((int) d.viewFor (0), (int) ProblemRowView::Collapsed);
            expectEquals ((int) d.viewFor (1), (int) ProblemRowView::Selected);
        }

        beginTest ("re-selecting the expanded row keeps it expanded");
        {
            ProblemRowDisclosure d;
            d.toggleExpand (1, 3);
            d.select (1, 3);
            expectEquals (d.expandedIndex, 1);
            expectEquals ((int) d.viewFor (1), (int) ProblemRowView::Expanded);
        }

        beginTest ("collapse leaves selection");
        {
            ProblemRowDisclosure d;
            d.toggleExpand (1, 3);
            d.collapse();
            expectEquals (d.selectedIndex, 1);
            expectEquals (d.expandedIndex, -1);
        }

        beginTest ("onCountChanged clamps and drops orphan expansion");
        {
            ProblemRowDisclosure d;
            d.toggleExpand (4, 5);
            d.onCountChanged (2);
            expectEquals (d.selectedIndex, 1);
            expectEquals (d.expandedIndex, -1);

            ProblemRowDisclosure empty;
            empty.select (0, 1);
            empty.onCountChanged (0);
            expectEquals (empty.selectedIndex, -1);
            expectEquals (empty.expandedIndex, -1);
        }

        beginTest ("restore rehydrates a single expanded selection");
        {
            ProblemRowDisclosure d;
            d.restore (2, 2, 4);
            expectEquals (d.selectedIndex, 2);
            expectEquals (d.expandedIndex, 2);

            d.restore (1, 2, 4); // expansion must belong to the selected row
            expectEquals (d.selectedIndex, 1);
            expectEquals (d.expandedIndex, -1);

            d.restore (-1, 0, 4);
            expectEquals (d.selectedIndex, -1);
            expectEquals (d.expandedIndex, -1);
        }
    }
};

static ProblemRowDisclosureTest problemRowDisclosureTest;

#endif
