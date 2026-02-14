/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'display'> = [ <display-outside> || <display-inside> ] | <display-listitem> | <display-internal> | <display-box> | <display-legacy> | <-webkit-display>
// NOTE: All <display-legacy> values are aliases of other values, so do not appear in the enum.
// https://drafts.csswg.org/css-display/#propdef-display
enum class DisplayType : uint8_t {
    // [ <display-outside> || <display-inside> ] and <-webkit-display>
    BlockFlow,            // Shortens to `block`
    BlockFlowRoot,        // Shortens to `flow-root`
    BlockTable,           // Shortens to `table`
    BlockFlex,            // Shortens to `flex`
    BlockGrid,            // Shortens to `grid`
    BlockGridLanes,       // Shortens to `grid-lanes`
    BlockRuby,
    BlockDeprecatedFlex,  // Shortens to `-webkit-box`
    InlineFlow,           // Shortens to `inline`
    InlineFlowRoot,       // Shortens to `inline-block`
    InlineTable,
    InlineFlex,
    InlineGrid,
    InlineGridLanes,
    InlineRuby,           // Shortens to `ruby`
    InlineDeprecatedFlex, // Shortens to `-webkit-inline-box`

    // <display-listitem>
    BlockFlowListItem,    // Shortens to `list-item`

    // <display-internal>
    TableCaption,
    TableCell,
    TableColumnGroup,
    TableColumn,
    TableHeaderGroup,
    TableFooterGroup,
    TableRow,
    TableRowGroup,
    RubyBase,
    RubyText,

    // <display-box>
    Contents,
    None
};

// https://drafts.csswg.org/css-display/#blockify
constexpr DisplayType blockify(DisplayType display)
{
    switch (display) {
    case DisplayType::BlockFlow:
    case DisplayType::BlockFlowRoot:
    case DisplayType::BlockTable:
    case DisplayType::BlockFlex:
    case DisplayType::BlockGrid:
    case DisplayType::BlockGridLanes:
    case DisplayType::BlockRuby:
    case DisplayType::BlockDeprecatedFlex:
    case DisplayType::BlockFlowListItem:
        return display;

    case DisplayType::InlineTable:
        return DisplayType::BlockTable;
    case DisplayType::InlineFlex:
        return DisplayType::BlockFlex;
    case DisplayType::InlineGrid:
        return DisplayType::BlockGrid;
    case DisplayType::InlineGridLanes:
        return DisplayType::BlockGridLanes;
    case DisplayType::InlineRuby:
        return DisplayType::BlockRuby;
    case DisplayType::InlineDeprecatedFlex:
        return DisplayType::BlockDeprecatedFlex;

    case DisplayType::InlineFlow:
    case DisplayType::InlineFlowRoot:
    case DisplayType::TableRowGroup:
    case DisplayType::TableHeaderGroup:
    case DisplayType::TableFooterGroup:
    case DisplayType::TableRow:
    case DisplayType::TableColumnGroup:
    case DisplayType::TableColumn:
    case DisplayType::TableCell:
    case DisplayType::TableCaption:
    case DisplayType::RubyBase:
    case DisplayType::RubyText:
        return DisplayType::BlockFlow;

    case DisplayType::Contents:
        ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
        return DisplayType::Contents;
    case DisplayType::None:
        ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
        return DisplayType::None;
    }

    RELEASE_ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
}

// https://drafts.csswg.org/css-display/#inlinify
constexpr DisplayType inlinify(DisplayType display)
{
    switch (display) {
    case DisplayType::BlockFlow:
        return DisplayType::InlineFlowRoot;
    case DisplayType::BlockTable:
        return DisplayType::InlineTable;
    case DisplayType::BlockFlex:
        return DisplayType::InlineFlex;
    case DisplayType::BlockGrid:
        return DisplayType::InlineGrid;
    case DisplayType::BlockGridLanes:
        return DisplayType::InlineGridLanes;
    case DisplayType::BlockRuby:
        return DisplayType::InlineRuby;
    case DisplayType::BlockDeprecatedFlex:
        return DisplayType::InlineDeprecatedFlex;

    case DisplayType::InlineFlow:
    case DisplayType::InlineFlowRoot:
    case DisplayType::InlineTable:
    case DisplayType::InlineFlex:
    case DisplayType::InlineGrid:
    case DisplayType::InlineGridLanes:
    case DisplayType::InlineRuby:
    case DisplayType::InlineDeprecatedFlex:
    case DisplayType::RubyBase:
    case DisplayType::RubyText:
        return display;

    case DisplayType::BlockFlowRoot:
    case DisplayType::BlockFlowListItem:
    case DisplayType::TableRowGroup:
    case DisplayType::TableHeaderGroup:
    case DisplayType::TableFooterGroup:
    case DisplayType::TableRow:
    case DisplayType::TableColumnGroup:
    case DisplayType::TableColumn:
    case DisplayType::TableCell:
    case DisplayType::TableCaption:
        return DisplayType::InlineFlow;

    case DisplayType::Contents:
        ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
        return DisplayType::Contents;
    case DisplayType::None:
        ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
        return DisplayType::None;
    }

    RELEASE_ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
}

constexpr bool isDisplayBlockType(DisplayType display)
{
    return display == DisplayType::BlockFlow
        || display == DisplayType::BlockFlowRoot
        || display == DisplayType::BlockTable
        || display == DisplayType::BlockFlex
        || display == DisplayType::BlockGrid
        || display == DisplayType::BlockGridLanes
        || display == DisplayType::BlockRuby
        || display == DisplayType::BlockDeprecatedFlex
        || display == DisplayType::BlockFlowListItem;
}

constexpr bool isDisplayInlineType(DisplayType display)
{
    return display == DisplayType::InlineFlow
        || display == DisplayType::InlineFlowRoot
        || display == DisplayType::InlineTable
        || display == DisplayType::InlineFlex
        || display == DisplayType::InlineGrid
        || display == DisplayType::InlineGridLanes
        || display == DisplayType::InlineRuby
        || display == DisplayType::InlineDeprecatedFlex
        || display == DisplayType::RubyBase
        || display == DisplayType::RubyText;
}

constexpr bool isDisplayTableOrTablePart(DisplayType display)
{
    return display == DisplayType::BlockTable
        || display == DisplayType::InlineTable
        || display == DisplayType::TableCell
        || display == DisplayType::TableCaption
        || display == DisplayType::TableRowGroup
        || display == DisplayType::TableHeaderGroup
        || display == DisplayType::TableFooterGroup
        || display == DisplayType::TableRow
        || display == DisplayType::TableColumnGroup
        || display == DisplayType::TableColumn;
}

// https://drafts.csswg.org/css-display/#internal-table-box
constexpr bool isInternalTableBox(DisplayType display)
{
    return display == DisplayType::TableCell
        || display == DisplayType::TableRowGroup
        || display == DisplayType::TableHeaderGroup
        || display == DisplayType::TableFooterGroup
        || display == DisplayType::TableRow
        || display == DisplayType::TableColumnGroup
        || display == DisplayType::TableColumn;
}

// https://drafts.csswg.org/css-display/#internal-ruby-box
constexpr bool isRubyContainerOrInternalRubyBox(DisplayType display)
{
    return display == DisplayType::InlineRuby
        || display == DisplayType::RubyText
        || display == DisplayType::RubyBase;
}

constexpr bool isDisplayGridBox(DisplayType display)
{
    return display == DisplayType::BlockGrid
        || display == DisplayType::InlineGrid;
}

constexpr bool isDisplayGridLanesBox(DisplayType display)
{
    return display == DisplayType::BlockGridLanes
        || display == DisplayType::InlineGridLanes;
}

constexpr bool isDisplayListItemType(DisplayType display)
{
    return display == DisplayType::BlockFlowListItem;
}

constexpr bool isDisplayDeprecatedFlexibleBox(DisplayType display)
{
    return display == DisplayType::BlockDeprecatedFlex
        || display == DisplayType::InlineDeprecatedFlex;
}

constexpr bool isDisplayFlexibleBox(DisplayType display)
{
    return display == DisplayType::BlockFlex
        || display == DisplayType::InlineFlex;
}

constexpr bool isDisplayGridFormattingContextBox(DisplayType display)
{
    return isDisplayGridBox(display)
        || isDisplayGridLanesBox(display);
}

constexpr bool isDisplayFlexibleOrGridFormattingContextBox(DisplayType display)
{
    return isDisplayFlexibleBox(display)
        || isDisplayGridFormattingContextBox(display);
}

constexpr bool isDisplayFlexibleBoxIncludingDeprecatedOrGridFormattingContextBox(DisplayType display)
{
    return isDisplayFlexibleOrGridFormattingContextBox(display)
        || isDisplayDeprecatedFlexibleBox(display);
}

constexpr bool doesDisplayGenerateBlockContainer(DisplayType display)
{
    return display == DisplayType::BlockFlow
        || display == DisplayType::BlockFlowRoot
        || display == DisplayType::BlockFlowListItem
        || display == DisplayType::InlineFlowRoot
        || display == DisplayType::TableCell
        || display == DisplayType::TableCaption;
}

// MARK: - Conversion

template<> struct CSSValueConversion<DisplayType> { auto operator()(BuilderState&, const CSSValue&) -> DisplayType; };

template<> struct ValueRepresentation<DisplayType> {
    template<typename... F> constexpr decltype(auto) operator()(DisplayType value, F&&... f)
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);
        switch (value) {
        // [ <display-outside> || <display-inside> ] and <-webkit-display>
        case DisplayType::BlockFlow:
            return visitor(CSS::Keyword::Block { });
        case DisplayType::BlockFlowRoot:
            return visitor(CSS::Keyword::FlowRoot { });
        case DisplayType::BlockTable:
            return visitor(CSS::Keyword::Table { });
        case DisplayType::BlockFlex:
            return visitor(CSS::Keyword::Flex { });
        case DisplayType::BlockGrid:
            return visitor(CSS::Keyword::Grid { });
        case DisplayType::BlockGridLanes:
            return visitor(CSS::Keyword::GridLanes { });
        case DisplayType::BlockRuby:
            return visitor(CSS::Keyword::BlockRuby { });
        case DisplayType::BlockDeprecatedFlex:
            return visitor(CSS::Keyword::WebkitBox { });

        case DisplayType::InlineFlow:
            return visitor(CSS::Keyword::Inline { });
        case DisplayType::InlineFlowRoot:
            return visitor(CSS::Keyword::InlineBlock { });
        case DisplayType::InlineTable:
            return visitor(CSS::Keyword::InlineTable { });
        case DisplayType::InlineFlex:
            return visitor(CSS::Keyword::InlineFlex { });
        case DisplayType::InlineGrid:
            return visitor(CSS::Keyword::InlineGrid { });
        case DisplayType::InlineGridLanes:
            return visitor(CSS::Keyword::InlineGridLanes { });
        case DisplayType::InlineRuby:
            return visitor(CSS::Keyword::Ruby { });
        case DisplayType::InlineDeprecatedFlex:
            return visitor(CSS::Keyword::WebkitInlineBox { });

        // <display-listitem>
        case DisplayType::BlockFlowListItem:
            return visitor(CSS::Keyword::ListItem { });

        // <display-internal>
        case DisplayType::TableRowGroup:
            return visitor(CSS::Keyword::TableRowGroup { });
        case DisplayType::TableHeaderGroup:
            return visitor(CSS::Keyword::TableHeaderGroup { });
        case DisplayType::TableFooterGroup:
            return visitor(CSS::Keyword::TableFooterGroup { });
        case DisplayType::TableRow:
            return visitor(CSS::Keyword::TableRow { });
        case DisplayType::TableColumnGroup:
            return visitor(CSS::Keyword::TableColumnGroup { });
        case DisplayType::TableColumn:
            return visitor(CSS::Keyword::TableColumn { });
        case DisplayType::TableCell:
            return visitor(CSS::Keyword::TableCell { });
        case DisplayType::TableCaption:
            return visitor(CSS::Keyword::TableCaption { });
        case DisplayType::RubyBase:
            return visitor(CSS::Keyword::RubyBase { });
        case DisplayType::RubyText:
            return visitor(CSS::Keyword::RubyText { });

        // <display-box>
        case DisplayType::None:
            return visitor(CSS::Keyword::None { });
        case DisplayType::Contents:
            return visitor(CSS::Keyword::Contents { });
        }

        RELEASE_ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    }
};

// MARK: - Blending

template<> struct Blending<DisplayType> {
    constexpr auto canBlend(DisplayType, DisplayType) -> bool { return false; }
    auto blend(DisplayType, DisplayType, const BlendingContext&) -> DisplayType;
};

} // namespace Style
} // namespace WebCore
