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
inline constexpr PropertyId Points{"points"};

// Values (elements and symbols)
inline constexpr PropertyId Value{"value"};
inline constexpr PropertyId Minimum{"minimum"};
inline constexpr PropertyId Maximum{"maximum"};
inline constexpr PropertyId Decimals{"decimals"};
inline constexpr PropertyId Unit{"unit"};
inline constexpr PropertyId IoMode{"ioMode"};
inline constexpr PropertyId Checked{"checked"};
inline constexpr PropertyId Items{"items"};
inline constexpr PropertyId SelectedIndex{"selectedIndex"};
inline constexpr PropertyId Orientation{"orientation"};
inline constexpr PropertyId OnText{"onText"};
inline constexpr PropertyId OffText{"offText"};
inline constexpr PropertyId OnColor{"onColor"};
inline constexpr PropertyId OffColor{"offColor"};
inline constexpr PropertyId BackgroundColor{"backgroundColor"};
inline constexpr PropertyId AccentColor{"accentColor"};
inline constexpr PropertyId ShowScale{"showScale"};
inline constexpr PropertyId ClockMode{"clockMode"};
inline constexpr PropertyId Action{"action"};
inline constexpr PropertyId ActionTag{"actionTag"};
inline constexpr PropertyId ActionValue{"actionValue"};

// Process symbols
inline constexpr PropertyId Running{"running"};
inline constexpr PropertyId Open{"open"};
inline constexpr PropertyId Flowing{"flowing"};
inline constexpr PropertyId FlowColor{"flowColor"};
inline constexpr PropertyId SensorKind{"sensorKind"};
inline constexpr PropertyId StretchMode{"stretchMode"};

// Controls
inline constexpr PropertyId Columns{"columns"};
inline constexpr PropertyId RowCount{"rowCount"};
inline constexpr PropertyId ShowToolbar{"showToolbar"};
inline constexpr PropertyId TimeRange{"timeRange"};
inline constexpr PropertyId ScreenName{"screenName"};
inline constexpr PropertyId Url{"url"};
inline constexpr PropertyId Source{"source"};
inline constexpr PropertyId ShowControls{"showControls"};
inline constexpr PropertyId RecipeName{"recipeName"};

// Graphics
inline constexpr PropertyId Asset{"asset"};
inline constexpr PropertyId FitMode{"fitMode"};
inline constexpr PropertyId StyleOverrides{"styleOverrides"};

// Text
inline constexpr PropertyId Text{"text"};
inline constexpr PropertyId Font{"font"};
inline constexpr PropertyId TextColor{"textColor"};
inline constexpr PropertyId HorizontalAlignment{"horizontalAlignment"};
inline constexpr PropertyId VerticalAlignment{"verticalAlignment"};
inline constexpr PropertyId WordWrap{"wordWrap"};

} // namespace hmi::Prop
