#include "editor/app/SampleProject.h"

#include "core/model/HmiObject.h"
#include "core/model/Project.h"
#include "core/model/Screen.h"
#include "core/model/ScreenBuilder.h"
#include "core/objects/IoField.h"
#include "core/objects/Pipe.h"
#include "core/objects/Pump.h"
#include "core/objects/Rectangle.h"
#include "core/objects/Tank.h"
#include "core/objects/TextBox.h"
#include "core/objects/Valve.h"
#include "core/property/Prop.h"
#include "core/tags/Binding.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>

#include <cmath>
#include <vector>

namespace hmi
{

namespace
{

QString tr(const char* text)
{
    return QCoreApplication::translate("hmi::SampleProject", text);
}

// The layout is drawn on a 1280 x 800 grid and scaled evenly to the screen
// (Screen::kDefaultSize), centred; bands as wide as the grid span the screen.
constexpr double kGridWidth = 1280.0;
constexpr double kScale = Screen::kDefaultSize.height() / 800.0;
constexpr double kOffset = (Screen::kDefaultSize.width() - kGridWidth * kScale) / 2.0;

QRectF at(double x, double y, double width, double height)
{
    if (width >= kGridWidth)
        return {0.0, y * kScale, Screen::kDefaultSize.width(), height * kScale};
    return {kOffset + x * kScale, y * kScale, width * kScale, height * kScale};
}

QFont fontOf(int pixelSize, bool bold = false)
{
    QFont f;
    f.setPixelSize(int(std::lround(pixelSize * kScale)));
    f.setBold(bold);
    return f;
}

Tag makeTag(const QString& name, Tag::Type type, const QString& unit, double minimum,
            double maximum, Tag::Simulation simulation, double period, const QString& comment)
{
    Tag tag;
    tag.name = name;
    tag.type = type;
    tag.unit = unit;
    tag.minimum = minimum;
    tag.maximum = maximum;
    tag.simulation = simulation;
    tag.period = period;
    tag.comment = comment;
    tag.initial = tag.startValue();
    return tag;
}

// A binding to set once the screen is built.
struct Link
{
    ObjectId object;
    PropertyId property;
    QString tag;
    QString kind = QStringLiteral("direct");
    QJsonObject parameters;
};

// Green below `limit`, then the alarm colour.
QJsonObject levelColours(double limit, const QString& normal, const QString& high)
{
    return {{QStringLiteral("ranges"), QJsonArray{QJsonObject{{QStringLiteral("from"), -1e9},
                                                              {QStringLiteral("to"), limit},
                                                              {QStringLiteral("value"), normal}}}},
            {QStringLiteral("default"), high}};
}

ObjectId valueCard(ScreenBuilder& b, double x, double y, const QString& label, const QString& unit,
                   int decimals)
{
    b.add(Rectangle::kTypeId, at(x, y, 230, 116))
        .with(Prop::FillColor, QColor(0xF4, 0xF5, 0xF7))
        .with(Prop::LineColor, QColor(0xC9, 0xCE, 0xD6))
        .with(Prop::CornerRadius, 6.0);
    b.add(TextBox::kTypeId, at(x + 20, y + 14, 190, 24))
        .with(Prop::Text, label)
        .with(Prop::Font, fontOf(16))
        .with(Prop::TextColor, QColor(0x4A, 0x50, 0x58));
    b.add(IoField::kTypeId, at(x + 20, y + 46, 190, 50))
        .with(Prop::Unit, unit)
        .with(Prop::Decimals, decimals)
        .with(Prop::Name, label);
    return b.lastId();
}

} // namespace

std::unique_ptr<Project> makeSampleProject()
{
    auto project = std::make_unique<Project>(tr("Line 3 · Filling Station"));
    std::vector<Link> links;

    ScreenBuilder b(tr("02 Filling"));
    b.size(Screen::kDefaultSize).background(QColor(0xE6, 0xE8, 0xEB));

    // Header
    b.add(Rectangle::kTypeId, at(0, 0, 1280, 68))
        .with(Prop::FillColor, QColor(0x2B, 0x2F, 0x36))
        .with(Prop::LineWidth, 0.0)
        .with(Prop::Name, tr("Header"));
    b.add(TextBox::kTypeId, at(28, 0, 600, 68))
        .with(Prop::Text, tr("Filling Station · Line 3"))
        .with(Prop::Font, fontOf(22, true))
        .with(Prop::TextColor, QColor(Qt::white));
    b.add(Rectangle::kTypeId, at(80, 96, 240, 420))
        .with(Prop::FillColor, QColor(0xF4, 0xF5, 0xF7))
        .with(Prop::LineColor, QColor(0xC9, 0xCE, 0xD6))
        .with(Prop::CornerRadius, 6.0)
        .with(Prop::Name, tr("T-101 panel"));

    // Tank T-101: its level, and red when it is high.
    b.add(TextBox::kTypeId, at(80, 104, 240, 36))
        .with(Prop::Text, tr("T-101"))
        .with(Prop::Font, fontOf(22, true))
        .with(Prop::HorizontalAlignment, EnumValue{TextBox::AlignHCenter});
    b.add(Tank::kTypeId, at(130, 146, 140, 280)).with(Prop::Name, tr("T-101"));
    const ObjectId tank = b.lastId();
    links.push_back({tank, Prop::Value, QStringLiteral("Level")});
    links.push_back({tank, Prop::FillColor, QStringLiteral("Level"), QStringLiteral("ranges"),
                     levelColours(85, QStringLiteral("#3B82F6"), QStringLiteral("#D64541"))});
    b.add(IoField::kTypeId, at(130, 440, 140, 48))
        .with(Prop::Unit, QStringLiteral("%"))
        .with(Prop::Decimals, 1)
        .with(Prop::Name, tr("T-101 level"));
    links.push_back({b.lastId(), Prop::Value, QStringLiteral("Level")});

    // Pipe from the tank through pump and valve to the filler; it flows
    // while the pump runs.
    b.add(Pipe::kTypeId, at(200, 516, 800, 64))
        .with(Prop::Points, PointList{{0.0, 0.0}, {0.0, 1.0}, {1.0, 1.0}})
        .with(Prop::Name, tr("Feed line"));
    links.push_back({b.lastId(), Prop::Flowing, QStringLiteral("PumpRunning")});
    b.add(Pump::kTypeId, at(460, 540, 80, 80)).with(Prop::Name, tr("P-101"));
    links.push_back({b.lastId(), Prop::Running, QStringLiteral("PumpRunning")});
    b.add(TextBox::kTypeId, at(420, 626, 160, 28))
        .with(Prop::Text, tr("P-101"))
        .with(Prop::Font, fontOf(16))
        .with(Prop::HorizontalAlignment, EnumValue{TextBox::AlignHCenter});
    b.add(Valve::kTypeId, at(720, 548, 64, 64)).with(Prop::Name, tr("V-101"));
    links.push_back({b.lastId(), Prop::Open, QStringLiteral("PumpRunning")});
    b.add(TextBox::kTypeId, at(672, 626, 160, 28))
        .with(Prop::Text, tr("V-101"))
        .with(Prop::Font, fontOf(16))
        .with(Prop::HorizontalAlignment, EnumValue{TextBox::AlignHCenter});
    b.add(Rectangle::kTypeId, at(1000, 530, 230, 100))
        .with(Prop::FillColor, QColor(0xF4, 0xF5, 0xF7))
        .with(Prop::LineWidth, 3.0)
        .with(Prop::CornerRadius, 6.0)
        .with(Prop::Name, tr("F-201"));
    b.add(TextBox::kTypeId, at(1020, 545, 200, 70))
        .with(Prop::Text, tr("F-201 Filler"))
        .with(Prop::Font, fontOf(20));

    links.push_back({valueCard(b, 360, 120, tr("Pressure"), tr("bar"), 2), Prop::Value,
                     QStringLiteral("Pressure")});
    links.push_back(
        {valueCard(b, 360, 256, tr("Flow"), tr("L/min"), 1), Prop::Value, QStringLiteral("Flow")});
    links.push_back({valueCard(b, 360, 392, tr("Temperature"), tr("°C"), 1), Prop::Value,
                     QStringLiteral("Temperature")});

    // Banner: its colour and message follow the level.
    b.add(Rectangle::kTypeId, at(0, 720, 1280, 80))
        .with(Prop::FillColor, QColor(0xF2, 0xA1, 0x3A))
        .with(Prop::LineWidth, 0.0)
        .with(Prop::Name, tr("Alarm banner"));
    links.push_back({b.lastId(), Prop::FillColor, QStringLiteral("Level"), QStringLiteral("ranges"),
                     levelColours(85, QStringLiteral("#3A9D5D"), QStringLiteral("#F2A13A"))});
    b.add(TextBox::kTypeId, at(28, 720, 900, 80))
        .with(Prop::Text, tr("Warning · T-101 level approaching high limit"))
        .with(Prop::Font, fontOf(20, true))
        .with(Prop::Name, tr("Alarm text"));
    links.push_back({b.lastId(), Prop::Text, QStringLiteral("Level"), QStringLiteral("ranges"),
                     levelColours(85, tr("T-101 level normal"),
                                  tr("Warning · T-101 level approaching high limit"))});

    auto inserted = project->insertScreen(b.build());
    Q_ASSERT(inserted);
    Screen* screen = *inserted;

    for (const Link& link : links)
    {
        auto transform = TransformRegistry::instance().create(link.kind, link.parameters);
        HmiObject* object = screen->object(link.object);
        Q_ASSERT(transform && object);
        [[maybe_unused]] auto bound =
            object->setBinding({link.property, link.tag, std::move(*transform)});
        Q_ASSERT(bound);
    }

    const Tag tags[] = {
        makeTag(QStringLiteral("Level"), Tag::Type::Real, QStringLiteral("%"), 0, 100,
                Tag::Simulation::Sine, 30, tr("T-101 level")),
        makeTag(QStringLiteral("PumpRunning"), Tag::Type::Bool, {}, 0, 1, Tag::Simulation::Toggle,
                16, tr("P-101 running")),
        makeTag(QStringLiteral("Pressure"), Tag::Type::Real, QStringLiteral("bar"), 1.8, 3.0,
                Tag::Simulation::Random, 2, tr("Line pressure")),
        makeTag(QStringLiteral("Flow"), Tag::Type::Real, QStringLiteral("L/min"), 0, 60,
                Tag::Simulation::Sine, 20, tr("Feed flow")),
        makeTag(QStringLiteral("Temperature"), Tag::Type::Real, QStringLiteral("°C"), 18, 26,
                Tag::Simulation::Ramp, 60, tr("Product temperature")),
    };
    for (const Tag& tag : tags)
    {
        [[maybe_unused]] auto added = project->tags().insert(tag);
        Q_ASSERT(added);
    }
    return project;
}

} // namespace hmi
