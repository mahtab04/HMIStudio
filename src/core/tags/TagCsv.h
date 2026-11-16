#pragma once

#include "core/Result.h"
#include "core/tags/Tag.h"

#include <QByteArray>

#include <span>
#include <vector>

namespace hmi
{

// Tags as CSV for spreadsheets: UTF-8, comma separated, one header row,
// fields quoted when they contain a comma, quote or line break:
//
//     name,type,unit,minimum,maximum,simulation,period,initial,comment
//     Tank_Level,real,%,0,100,sine,30,,Level of T-101
//
// Columns may come in any order; unknown columns are ignored, missing ones
// take the defaults. A semicolon also separates fields when the header uses
// it (spreadsheets in many locales write that).
namespace TagCsv
{

[[nodiscard]] QByteArray write(std::span<const Tag> tags);
[[nodiscard]] Result<std::vector<Tag>> read(const QByteArray& csv);

} // namespace TagCsv

} // namespace hmi
