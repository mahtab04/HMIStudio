#include "core/tags/Expression.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace hmi
{

struct Expression::Node
{
    enum class Kind
    {
        Number,
        Variable,
        Negate,
        Not,
        Add,
        Subtract,
        Multiply,
        Divide,
        Modulo,
        Less,
        LessEqual,
        Greater,
        GreaterEqual,
        Equal,
        NotEqual,
        And,
        Or,
        Choose, // a ? b : c
        Function,
    };

    Kind kind = Kind::Number;
    double number = 0.0;
    QString function;
    std::vector<std::shared_ptr<const Node>> args;

    [[nodiscard]] double evaluate(double x) const
    {
        const auto arg = [&](std::size_t i)
        {
            return args[i]->evaluate(x);
        };
        switch (kind)
        {
        case Kind::Number:
            return number;
        case Kind::Variable:
            return x;
        case Kind::Negate:
            return -arg(0);
        case Kind::Not:
            return arg(0) == 0.0 ? 1.0 : 0.0;
        case Kind::Add:
            return arg(0) + arg(1);
        case Kind::Subtract:
            return arg(0) - arg(1);
        case Kind::Multiply:
            return arg(0) * arg(1);
        case Kind::Divide:
            return arg(0) / arg(1);
        case Kind::Modulo:
            return std::fmod(arg(0), arg(1));
        case Kind::Less:
            return arg(0) < arg(1) ? 1.0 : 0.0;
        case Kind::LessEqual:
            return arg(0) <= arg(1) ? 1.0 : 0.0;
        case Kind::Greater:
            return arg(0) > arg(1) ? 1.0 : 0.0;
        case Kind::GreaterEqual:
            return arg(0) >= arg(1) ? 1.0 : 0.0;
        case Kind::Equal:
            return arg(0) == arg(1) ? 1.0 : 0.0;
        case Kind::NotEqual:
            return arg(0) != arg(1) ? 1.0 : 0.0;
        case Kind::And:
            return arg(0) != 0.0 && arg(1) != 0.0 ? 1.0 : 0.0;
        case Kind::Or:
            return arg(0) != 0.0 || arg(1) != 0.0 ? 1.0 : 0.0;
        case Kind::Choose:
            return arg(0) != 0.0 ? arg(1) : arg(2);
        case Kind::Function:
            return call(x);
        }
        return 0.0;
    }

    [[nodiscard]] double call(double x) const
    {
        std::vector<double> v;
        v.reserve(args.size());
        for (const auto& a : args)
            v.push_back(a->evaluate(x));
        if (function == u"abs")
            return std::abs(v[0]);
        if (function == u"round")
            return std::round(v[0]);
        if (function == u"floor")
            return std::floor(v[0]);
        if (function == u"ceil")
            return std::ceil(v[0]);
        if (function == u"sqrt")
            return std::sqrt(v[0]);
        if (function == u"pow")
            return std::pow(v[0], v[1]);
        if (function == u"min")
            return *std::ranges::min_element(v);
        if (function == u"max")
            return *std::ranges::max_element(v);
        if (function == u"clamp")
            return std::clamp(v[0], std::min(v[1], v[2]), std::max(v[1], v[2]));
        return 0.0;
    }
};

namespace
{

using Node = Expression::Node;
using NodePtr = std::shared_ptr<const Node>;

// Arity of each known function; -1 for "one or more".
int arityOf(const QString& name)
{
    if (name == u"abs" || name == u"round" || name == u"floor" || name == u"ceil" ||
        name == u"sqrt")
        return 1;
    if (name == u"pow")
        return 2;
    if (name == u"clamp")
        return 3;
    if (name == u"min" || name == u"max")
        return -1;
    return 0;
}

class Parser
{
public:
    explicit Parser(const QString& text)
        : m_text(text)
    {
    }

    Result<NodePtr> parseAll()
    {
        auto root = expression();
        if (!root)
            return root;
        skipSpace();
        if (m_pos < m_text.size())
            return fail(QStringLiteral("unexpected '%1'").arg(m_text.at(m_pos)));
        return root;
    }

private:
    tl::unexpected<Error> fail(const QString& what) const
    {
        return makeError(ErrorCode::InvalidArgument,
                         QStringLiteral("Expression: %1 at position %2").arg(what).arg(m_pos + 1));
    }

    void skipSpace()
    {
        while (m_pos < m_text.size() && m_text.at(m_pos).isSpace())
            ++m_pos;
    }

    bool accept(QStringView token)
    {
        skipSpace();
        if (QStringView(m_text).mid(m_pos).startsWith(token))
        {
            m_pos += token.size();
            return true;
        }
        return false;
    }

    static NodePtr make(Node::Kind kind, std::vector<NodePtr> args)
    {
        auto node = std::make_shared<Node>();
        node->kind = kind;
        node->args = std::move(args);
        return node;
    }

    Result<NodePtr> expression()
    {
        auto condition = orExpression();
        if (!condition)
            return condition;
        if (!accept(u"?"))
            return condition;
        auto yes = expression();
        if (!yes)
            return yes;
        if (!accept(u":"))
            return fail(QStringLiteral("':' expected"));
        auto no = expression();
        if (!no)
            return no;
        return make(Node::Kind::Choose, {*condition, *yes, *no});
    }

    Result<NodePtr> orExpression()
    {
        auto left = andExpression();
        while (left && accept(u"||"))
        {
            auto right = andExpression();
            if (!right)
                return right;
            left = make(Node::Kind::Or, {*left, *right});
        }
        return left;
    }

    Result<NodePtr> andExpression()
    {
        auto left = comparison();
        while (left && accept(u"&&"))
        {
            auto right = comparison();
            if (!right)
                return right;
            left = make(Node::Kind::And, {*left, *right});
        }
        return left;
    }

    Result<NodePtr> comparison()
    {
        auto left = sum();
        if (!left)
            return left;
        // Longer operators first, so "<=" is not read as "<".
        static const std::pair<const char16_t*, Node::Kind> kOperators[] = {
            {u"<=", Node::Kind::LessEqual}, {u">=", Node::Kind::GreaterEqual},
            {u"==", Node::Kind::Equal},     {u"!=", Node::Kind::NotEqual},
            {u"<", Node::Kind::Less},       {u">", Node::Kind::Greater},
        };
        for (const auto& [token, kind] : kOperators)
        {
            if (accept(token))
            {
                auto right = sum();
                if (!right)
                    return right;
                return make(kind, {*left, *right});
            }
        }
        return left;
    }

    Result<NodePtr> sum()
    {
        auto left = product();
        while (left)
        {
            Node::Kind kind;
            if (accept(u"+"))
                kind = Node::Kind::Add;
            else if (accept(u"-"))
                kind = Node::Kind::Subtract;
            else
                break;
            auto right = product();
            if (!right)
                return right;
            left = make(kind, {*left, *right});
        }
        return left;
    }

    Result<NodePtr> product()
    {
        auto left = unary();
        while (left)
        {
            Node::Kind kind;
            if (accept(u"*"))
                kind = Node::Kind::Multiply;
            else if (accept(u"/"))
                kind = Node::Kind::Divide;
            else if (accept(u"%"))
                kind = Node::Kind::Modulo;
            else
                break;
            auto right = unary();
            if (!right)
                return right;
            left = make(kind, {*left, *right});
        }
        return left;
    }

    Result<NodePtr> unary()
    {
        if (accept(u"-"))
        {
            auto operand = unary();
            if (!operand)
                return operand;
            return make(Node::Kind::Negate, {*operand});
        }
        skipSpace();
        // "!" but not "!=".
        if (m_pos < m_text.size() && m_text.at(m_pos) == u'!' &&
            !(m_pos + 1 < m_text.size() && m_text.at(m_pos + 1) == u'='))
        {
            ++m_pos;
            auto operand = unary();
            if (!operand)
                return operand;
            return make(Node::Kind::Not, {*operand});
        }
        return primary();
    }

    Result<NodePtr> primary()
    {
        skipSpace();
        if (m_pos >= m_text.size())
            return fail(QStringLiteral("value expected"));
        if (accept(u"("))
        {
            auto inner = expression();
            if (!inner)
                return inner;
            if (!accept(u")"))
                return fail(QStringLiteral("')' expected"));
            return inner;
        }
        const QChar c = m_text.at(m_pos);
        if (c.isDigit() || c == u'.')
        {
            const qsizetype start = m_pos;
            while (m_pos < m_text.size() &&
                   (m_text.at(m_pos).isDigit() || m_text.at(m_pos) == u'.' ||
                    m_text.at(m_pos) == u'e' || m_text.at(m_pos) == u'E' ||
                    ((m_text.at(m_pos) == u'-' || m_text.at(m_pos) == u'+') && m_pos > start &&
                     (m_text.at(m_pos - 1) == u'e' || m_text.at(m_pos - 1) == u'E'))))
                ++m_pos;
            bool ok = false;
            const double value = m_text.mid(start, m_pos - start).toDouble(&ok);
            if (!ok)
                return fail(QStringLiteral("bad number"));
            auto node = std::make_shared<Node>();
            node->number = value;
            return NodePtr(node);
        }
        if (c.isLetter())
        {
            const qsizetype start = m_pos;
            while (m_pos < m_text.size() &&
                   (m_text.at(m_pos).isLetterOrNumber() || m_text.at(m_pos) == u'_'))
                ++m_pos;
            const QString name = m_text.mid(start, m_pos - start).toLower();
            if (name == u"x" || name == u"value")
                return make(Node::Kind::Variable, {});
            if (name == u"true" || name == u"false")
            {
                auto node = std::make_shared<Node>();
                node->number = name == u"true" ? 1.0 : 0.0;
                return NodePtr(node);
            }
            const int arity = arityOf(name);
            if (arity == 0)
                return fail(QStringLiteral("unknown name '%1'").arg(name));
            if (!accept(u"("))
                return fail(QStringLiteral("'(' expected after %1").arg(name));
            std::vector<NodePtr> args;
            if (!accept(u")"))
            {
                do
                {
                    auto a = expression();
                    if (!a)
                        return a;
                    args.push_back(*a);
                } while (accept(u","));
                if (!accept(u")"))
                    return fail(QStringLiteral("')' expected"));
            }
            if ((arity > 0 && int(args.size()) != arity) || (arity < 0 && args.empty()))
                return fail(QStringLiteral("%1 takes %2")
                                .arg(name)
                                .arg(arity < 0 ? QStringLiteral("one or more values")
                                               : QStringLiteral("%1 value(s)").arg(arity)));
            auto node = std::make_shared<Node>();
            node->kind = Node::Kind::Function;
            node->function = name;
            node->args = std::move(args);
            return NodePtr(node);
        }
        return fail(QStringLiteral("unexpected '%1'").arg(c));
    }

    const QString& m_text;
    qsizetype m_pos = 0;
};

} // namespace

Expression::Expression(QString text, std::shared_ptr<const Node> root)
    : m_text(std::move(text))
    , m_root(std::move(root))
{
}

Result<Expression> Expression::parse(const QString& text)
{
    Parser parser(text);
    auto root = parser.parseAll();
    if (!root)
        return tl::unexpected(root.error());
    return Expression(text, *root);
}

double Expression::evaluate(double x) const
{
    return m_root ? m_root->evaluate(x) : 0.0;
}

} // namespace hmi
