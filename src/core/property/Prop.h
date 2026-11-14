#pragma once

#include "core/model/Ids.h"

// Property ids shared across object types. An id names a meaning ("fill
// colour"), not a slot in one class, so the property panel can merge the same
// property across a mixed selection. Type-specific ids live here too, so two
// types never pick different ids for the same meaning.
namespace hmi::Prop
{

// Common to every object
inline constexpr PropertyId Name{"name"};
inline constexpr PropertyId X{"x"};
inline constexpr PropertyId Y{"y"};
inline constexpr PropertyId Width{"width"};
inline constexpr PropertyId Height{"height"};
inline constexpr PropertyId Rotation{"rotation"};
inline constexpr PropertyId Visible{"visible"};
inline constexpr PropertyId Locked{"locked"};

// Appearance
inline constexpr PropertyId FillColor{"fillColor"};
inline constexpr PropertyId LineColor{"lineColor"};
inline constexpr PropertyId LineWidth{"lineWidth"};
inline constexpr PropertyId CornerRadius{"cornerRadius"};
inline constexpr PropertyId LineDirection{"lineDirection"};

// Text
inline constexpr PropertyId Text{"text"};
inline constexpr PropertyId Font{"font"};
inline constexpr PropertyId TextColor{"textColor"};
inline constexpr PropertyId HorizontalAlignment{"horizontalAlignment"};
inline constexpr PropertyId VerticalAlignment{"verticalAlignment"};
inline constexpr PropertyId WordWrap{"wordWrap"};

} // namespace hmi::Prop
