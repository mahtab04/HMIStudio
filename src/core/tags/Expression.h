#pragma once

#include "core/Result.h"

#include <QString>

#include <memory>

namespace hmi
{

// A small arithmetic expression over the tag value `x` (also `value`),
// used by the expression transform:
//
//     x * 1.8 + 32        x > 80 ? 1 : 0        clamp(round(x / 10) * 10, 0, 100)
//
// Numbers, + - * / %, comparisons (< <= > >= == !=), && || !, ?:, and the
// functions abs, min, max, round, floor, ceil, sqrt, pow, clamp. True is 1,
// false is 0. It is parsed once; evaluating cannot fail (a division by zero
// gives infinity, which a binding then rejects). No script engine: a project
// file cannot run code.
class Expression
{
public:
    [[nodiscard]] static Result<Expression> parse(const QString& text);

    [[nodiscard]] double evaluate(double x) const;
    [[nodiscard]] const QString& text() const noexcept { return m_text; }

    struct Node;

private:
    Expression(QString text, std::shared_ptr<const Node> root);

    QString m_text;
    std::shared_ptr<const Node> m_root;
};

} // namespace hmi
