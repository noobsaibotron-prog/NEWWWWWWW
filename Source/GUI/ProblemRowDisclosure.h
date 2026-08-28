#pragma once

namespace EmberUI
{

/** Visual density of one AI problem row.

    Collapsed is the default list. Selected reveals the in-row action strip.
    Expanded is Selected plus the single detail inspector — never more than
    one row at a time.
*/
enum class ProblemRowView : unsigned char
{
    Collapsed = 0,
    Selected,
    Expanded
};

/** Selection / expansion state for the AI problem list.

    Pure index logic, no painting, no AI ranking, no apply behaviour.
    The panel rebinds indices after a list identity refresh.
*/
struct ProblemRowDisclosure
{
    int selectedIndex = -1;
    int expandedIndex = -1;

    void reset() noexcept
    {
        selectedIndex = -1;
        expandedIndex = -1;
    }

    void select (int index, int count) noexcept
    {
        if (index < 0 || index >= count)
        {
            reset();
            return;
        }

        if (expandedIndex != index)
            expandedIndex = -1;

        selectedIndex = index;
    }

    void toggleExpand (int index, int count) noexcept
    {
        if (index < 0 || index >= count)
            return;

        selectedIndex = index;
        expandedIndex = (expandedIndex == index) ? -1 : index;
    }

    void collapse() noexcept
    {
        expandedIndex = -1;
    }

    void restore (int selected, int expanded, int count) noexcept
    {
        select (selected, count);
        if (selectedIndex >= 0 && expanded == selectedIndex)
            expandedIndex = selectedIndex;
    }

    void onCountChanged (int count) noexcept
    {
        if (count <= 0)
        {
            reset();
            return;
        }

        if (selectedIndex >= count)
            selectedIndex = count - 1;
        if (expandedIndex >= count)
            expandedIndex = -1;
        if (expandedIndex >= 0 && expandedIndex != selectedIndex)
            expandedIndex = -1;
    }

    [[nodiscard]] ProblemRowView viewFor (int index) const noexcept
    {
        if (index < 0)
            return ProblemRowView::Collapsed;
        if (index == expandedIndex)
            return ProblemRowView::Expanded;
        if (index == selectedIndex)
            return ProblemRowView::Selected;
        return ProblemRowView::Collapsed;
    }

    [[nodiscard]] bool hasExpansion() const noexcept { return expandedIndex >= 0; }
};

} // namespace EmberUI
