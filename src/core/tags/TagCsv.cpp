#include "core/tags/TagCsv.h"

#include <QStringList>

namespace hmi::TagCsv
{

namespace
{

const QStringList kColumns{
    QStringLiteral("name"),          QStringLiteral("type"),       QStringLiteral("unit"),
    QStringLiteral("minimum"),       QStringLiteral("maximum"),    QStringLiteral("simulation"),
    QStringLiteral("period"),        QStringLiteral("initial"),    QStringLiteral("comment"),
    QStringLiteral("table"),         QStringLiteral("connection"), QStringLiteral("address"),
    QStringLiteral("acquisitionMs"), QStringLiteral("limitInput"),
};

QString quoted(const QString& field, QChar separator)
{
    if (!field.contains(separator) && !field.contains(u'"') && !field.contains(u'\n') &&
        !field.contains(u'\r'))
        return field;
    QString escaped = field;
    escaped.replace(QStringLiteral("\""), QStringLiteral("\"\""));
    return u'"' + escaped + u'"';
}

// Splits CSV text into rows of fields, honouring quotes.
std::vector<QStringList> parse(const QString& text, QChar separator)
{
    std::vector<QStringList> rows;
    QStringList row;
    QString field;
    bool inQuotes = false;
    for (qsizetype i = 0; i < text.size(); ++i)
    {
        const QChar c = text.at(i);
        if (inQuotes)
        {
            if (c == u'"' && i + 1 < text.size() && text.at(i + 1) == u'"')
            {
                field += u'"';
                ++i;
            }
            else if (c == u'"')
            {
                inQuotes = false;
            }
            else
            {
                field += c;
            }
        }
        else if (c == u'"')
        {
            inQuotes = true;
        }
        else if (c == separator)
        {
            row << field;
            field.clear();
        }
        else if (c == u'\n' || c == u'\r')
        {
            if (c == u'\r' && i + 1 < text.size() && text.at(i + 1) == u'\n')
                ++i;
            row << field;
            field.clear();
            if (!(row.size() == 1 && row.front().isEmpty()))
                rows.push_back(row);
            row.clear();
        }
        else
        {
            field += c;
        }
    }
    if (!field.isEmpty() || !row.isEmpty())
    {
        row << field;
        rows.push_back(row);
    }
    return rows;
}

} // namespace

QByteArray write(std::span<const Tag> tags)
{
    const QChar separator = u',';
    QString text = kColumns.join(separator) + u'\n';
    for (const Tag& tag : tags)
    {
        const QStringList fields{
            tag.name,
            Tag::typeKey(tag.type),
            tag.unit,
            QString::number(tag.minimum, 'g', 15),
            QString::number(tag.maximum, 'g', 15),
            Tag::simulationKey(tag.simulation),
            QString::number(tag.period, 'g', 15),
            TagValues::toText(tag.initial),
            tag.comment,
            tag.table,
            tag.connection,
            tag.address,
            QString::number(tag.acquisitionMs),
            tag.limitInput ? QStringLiteral("true") : QStringLiteral("false"),
        };
        QStringList out;
        for (const QString& f : fields)
            out << quoted(f, separator);
        text += out.join(separator) + u'\n';
    }
    return text.toUtf8();
}

Result<std::vector<Tag>> read(const QByteArray& csv)
{
    QString text = QString::fromUtf8(csv);
    if (text.startsWith(QChar(0xFEFF))) // byte order mark from spreadsheets
        text.remove(0, 1);
    const QString firstLine = text.section(u'\n', 0, 0);
    const QChar separator = firstLine.count(u';') > firstLine.count(u',') ? u';' : u',';
    const auto rows = parse(text, separator);
    if (rows.empty())
        return makeError(ErrorCode::BadFormat, QStringLiteral("The file has no header row"));

    QStringList header;
    for (const QString& h : rows.front())
        header << h.trimmed().toLower();
    const qsizetype nameColumn = header.indexOf(u"name");
    if (nameColumn < 0)
        return makeError(ErrorCode::BadFormat, QStringLiteral("The header has no 'name' column"));
    const auto field = [&header](const QStringList& row, const char* column) -> QString
    {
        const qsizetype i = header.indexOf(QLatin1StringView(column));
        return i >= 0 && i < row.size() ? row.at(i).trimmed() : QString();
    };

    std::vector<Tag> tags;
    for (std::size_t r = 1; r < rows.size(); ++r)
    {
        const QStringList& row = rows[r];
        const auto fail = [r](const QString& what)
        {
            return makeError(ErrorCode::BadFormat,
                             QStringLiteral("Row %1: %2").arg(r + 1).arg(what));
        };
        Tag tag;
        tag.name = field(row, "name");
        if (!Tag::isValidName(tag.name))
            return fail(QStringLiteral("missing or invalid name"));
        if (const QString t = field(row, "type"); !t.isEmpty())
        {
            const auto type = Tag::typeFromKey(t);
            if (!type)
                return fail(QStringLiteral("unknown type '%1'").arg(t));
            tag.type = *type;
        }
        tag.unit = field(row, "unit");
        tag.comment = field(row, "comment");
        tag.connection = field(row, "connection");
        tag.address = field(row, "address");
        if (const QString table = field(row, "table"); !table.isEmpty())
            tag.table = table;
        if (const QString limit = field(row, "limitinput").trimmed().toLower(); !limit.isEmpty())
        {
            if (limit != u"true" && limit != u"false" && limit != u"1" && limit != u"0")
                return fail(QStringLiteral("limitInput must be true or false"));
            tag.limitInput = limit == u"true" || limit == u"1";
        }
        if (const QString cycle = field(row, "acquisitionms"); !cycle.isEmpty())
        {
            bool valid = false;
            tag.acquisitionMs = cycle.toInt(&valid);
            if (!valid || tag.acquisitionMs < 100 || tag.acquisitionMs > 60000)
                return fail(QStringLiteral("acquisitionMs must be between 100 and 60000"));
        }
        bool ok = true;
        const auto number = [&](const char* column, double fallback)
        {
            const QString t = field(row, column);
            if (t.isEmpty())
                return fallback;
            bool good = false;
            const double v = t.toDouble(&good);
            ok = ok && good;
            return v;
        };
        tag.minimum = number("minimum", tag.minimum);
        tag.maximum = number("maximum", tag.maximum);
        tag.period = number("period", tag.period);
        if (!ok || !(tag.period > 0.0))
            return fail(QStringLiteral("minimum, maximum or period is not a number"));
        if (const QString s = field(row, "simulation"); !s.isEmpty())
        {
            const auto simulation = Tag::simulationFromKey(s);
            if (!simulation)
                return fail(QStringLiteral("unknown simulation '%1'").arg(s));
            tag.simulation = *simulation;
        }
        if (const QString initial = field(row, "initial"); !initial.isEmpty())
            tag.initial = tag.converted(TagValue(initial));
        tags.push_back(std::move(tag));
    }
    return tags;
}

} // namespace hmi::TagCsv
