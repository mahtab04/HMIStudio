#include "editor/app/SampleProject.h"

#include "core/model/ScreenBuilder.h"
#include "core/objects/Bar.h"
#include "core/objects/Button.h"
#include "core/objects/CheckBox.h"
#include "core/objects/Gauge.h"
#include "core/objects/IoField.h"
#include "core/objects/Pipe.h"
#include "core/objects/Pump.h"
#include "core/objects/Rectangle.h"
#include "core/objects/Slider.h"
#include "core/objects/Switch.h"
#include "core/objects/Tank.h"
#include "core/objects/TextBox.h"
#include "core/objects/Valve.h"
#include "core/property/Prop.h"

#include <QCoreApplication>
#include <QJsonArray>

#include <algorithm>
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
constexpr double kScale = Screen::kDefaultSize.height() / 800.0;
constexpr double kOffset = (Screen::kDefaultSize.width() - 1280.0 * kScale) / 2.0;
const QColor kInk(0x21, 0x32, 0x43);
const QColor kMuted(0x53, 0x65, 0x76);
const QColor kOutline(0xCF, 0xD8, 0xE0);
const QColor kGreen(0x17, 0x6B, 0x4D);
const QColor kRed(0xB5, 0x32, 0x38);
const QColor kBlue(0x27, 0x63, 0xA5);

QRectF at(double x, double y, double w, double h)
{
    if (w >= 1280)
        return {0, y * kScale, Screen::kDefaultSize.width(), h * kScale};
    return {kOffset + x * kScale, y * kScale, w * kScale, h * kScale};
}
QFont fontOf(int size, bool bold = false)
{
    QFont f;
    f.setPixelSize(int(std::lround(size * kScale)));
    f.setBold(bold);
    return f;
}
// Event actions to set once a screen is built.
struct EventLink
{
    ObjectId object;
    EventKind event;
    ActionList actions;
};

Action setTag(const QString& tag, const QString& value)
{
    return {.kind = Action::Kind::SetTag, .tag = tag, .value = value, .screen = {}};
}
Action incrementTag(const QString& tag, const QString& step)
{
    return {.kind = Action::Kind::IncrementTag, .tag = tag, .value = step, .screen = {}};
}
Action openScreen(const QString& screen)
{
    return {.kind = Action::Kind::OpenScreen, .tag = {}, .value = {}, .screen = screen};
}
Action showPopup(const QString& screen)
{
    return {.kind = Action::Kind::ShowPopup, .tag = {}, .value = {}, .screen = screen};
}
Action closePopup()
{
    return {.kind = Action::Kind::ClosePopup, .tag = {}, .value = {}, .screen = {}};
}

struct Link
{
    ObjectId object;
    PropertyId property;
    QString tag;
    QString kind = QStringLiteral("direct");
    QJsonObject parameters;
};

void label(ScreenBuilder& b, const QRectF& rect, const QString& text, int size = 16,
           const QColor& color = kInk, bool bold = false)
{
    b.add(TextBox::kTypeId, rect)
        .with(Prop::Text, text)
        .with(Prop::Font, fontOf(size, bold))
        .with(Prop::TextColor, color);
}
void panel(ScreenBuilder& b, const QRectF& rect)
{
    b.add(Rectangle::kTypeId, rect)
        .with(Prop::FillColor, QColor(Qt::white))
        .with(Prop::LineColor, kOutline)
        .with(Prop::CornerRadius, 8.0);
}
// A button whose Click runs `click`; `press` and `release` for hold buttons.
void button(ScreenBuilder& b, std::vector<EventLink>& events, const QRectF& rect,
            const QString& text, const QString& name, const QColor& color, ActionList click,
            ActionList press = {}, ActionList release = {})
{
    // Button painting scales its font with the control. Store a base font
    // that yields the same readable label size on each sample button.
    QFont font = fontOf(18, true);
    const double scale = std::clamp(std::min(rect.width() / 120.0, rect.height() / 40.0), 0.5, 8.0);
    font.setPixelSize(int(std::lround(font.pixelSize() / scale)));
    b.add(Button::kTypeId, rect)
        .with(Prop::Text, text)
        .with(Prop::Name, name)
        .with(Prop::FillColor, color)
        .with(Prop::LineColor, color.darker(115))
        .with(Prop::TextColor, QColor(Qt::white))
        .with(Prop::Font, font);
    const ObjectId id = b.lastId();
    if (!click.empty())
        events.push_back({id, EventKind::Click, std::move(click)});
    if (!press.empty())
        events.push_back({id, EventKind::Press, std::move(press)});
    if (!release.empty())
        events.push_back({id, EventKind::Release, std::move(release)});
}

// Applies bindings and event actions to a screen once it is in the project.
void wire(Screen& screen, const std::vector<Link>& links, const std::vector<EventLink>& events)
{
    for (const Link& link : links)
    {
        auto transform = TransformRegistry::instance().create(link.kind, link.parameters);
        Q_ASSERT(transform);
        [[maybe_unused]] const auto bound =
            screen.object(link.object)->setBinding({link.property, link.tag, *transform});
        Q_ASSERT(bound);
    }
    for (const EventLink& event : events)
        screen.object(event.object)->setActions(event.event, event.actions);
}

// Header band shared by the screens; `next` opens the other screen.
void header(ScreenBuilder& b, std::vector<EventLink>& events, const QString& title,
            const QString& subtitle, const QString& nextLabel, const QString& next)
{
    b.size(Screen::kDefaultSize).background(QColor(0xEB, 0xEF, 0xF3));
    b.add(Rectangle::kTypeId, at(0, 0, 1280, 80))
        .with(Prop::FillColor, QColor(0x1C, 0x30, 0x43))
        .with(Prop::LineWidth, 0.0)
        .with(Prop::Name, QCoreApplication::translate("hmi::SampleProject", "Header"));
    label(b, at(24, 12, 760, 36), title, 24, QColor(Qt::white), true);
    label(b, at(24, 48, 760, 26), subtitle, 16, QColor(0xC8, 0xD6, 0xE3));
    button(b, events, at(800, 18, 180, 44), nextLabel, nextLabel, QColor(0x3A, 0x55, 0x6E),
           {openScreen(next)});
    label(b, at(1000, 22, 250, 40),
          QCoreApplication::translate("hmi::SampleProject", "TRAINING SIMULATION"), 16,
          QColor(0xEE, 0xCA, 0x7A), true);
}
QJsonObject ranges(double limit, const QJsonValue& low, const QJsonValue& high)
{
    return {{QStringLiteral("ranges"), QJsonArray{QJsonObject{{QStringLiteral("from"), -1e9},
                                                              {QStringLiteral("to"), limit},
                                                              {QStringLiteral("value"), low}}}},
            {QStringLiteral("default"), high}};
}
void readout(ScreenBuilder& b, std::vector<Link>& links, double x, const QString& title,
             const QString& tag, const QString& unit, int decimals)
{
    label(b, at(x, 518, 210, 28), title, 16, kMuted);
    b.add(IoField::kTypeId, at(x, 550, 210, 48))
        .with(Prop::Name, title)
        .with(Prop::Unit, unit)
        .with(Prop::Decimals, decimals)
        .with(Prop::IoMode, EnumValue{IoField::Output});
    links.push_back({b.lastId(), Prop::Value, tag});
}
Tag tag(const QString& name, Tag::Type type, double low, double high, TagValue initial,
        const QString& table, const QString& comment, const QString& unit = {})
{
    Tag t;
    t.name = name;
    t.type = type;
    t.minimum = low;
    t.maximum = high;
    t.initial = std::move(initial);
    t.table = table;
    t.comment = comment;
    t.unit = unit;
    return t;
}
} // namespace

std::unique_ptr<Project> makeSampleProject()
{
    auto project = std::make_unique<Project>(tr("Line 3 - Buffer Filling Station"));
    [[maybe_unused]] const auto profile =
        project->setSimulationProfile(QStringLiteral("filling-station"));
    Q_ASSERT(profile);
    std::vector<Link> links;
    std::vector<EventLink> events;
    const QString filling = tr("02 Filling");
    const QString overview = tr("01 Overview");
    ScreenBuilder b(filling);
    header(b, events, tr("LINE 03  /  BUFFER FILLING"),
           tr("P-101 transfer pump   /   T-101 buffer tank"), tr("< OVERVIEW"), overview);

    panel(b, at(24, 96, 1232, 48));
    label(b, at(40, 104, 620, 32), tr("STOPPED - press Start to transfer product"), 18, kInk, true);
    links.push_back(
        {b.lastId(), Prop::Text, QStringLiteral("PumpRunning"), QStringLiteral("ranges"),
         ranges(0.5, tr("STOPPED - pump is off"), tr("RUNNING - transferring product to T-101"))});
    label(b, at(680, 104, 550, 32), tr("T-101 level normal"), 16, kMuted);
    links.push_back(
        {b.lastId(), Prop::Text, QStringLiteral("Level"), QStringLiteral("ranges"),
         ranges(90, tr("T-101 level normal"), tr("HIGH LEVEL - pump interlock active"))});

    panel(b, at(24, 164, 764, 450));
    label(b, at(48, 180, 700, 32), tr("PROCESS OVERVIEW"), 18, kInk, true);
    label(b, at(48, 220, 700, 28), tr("Supply > P-101 > V-101 > T-101 > downstream demand"), 16,
          kMuted);
    const QString details = tr("P-101 details");
    button(b, events, at(48, 420, 150, 40), tr("P-101 DETAILS"), tr("Show pump details"),
           QColor(0x3A, 0x55, 0x6E), {showPopup(details)});
    panel(b, at(48, 310, 116, 90));
    label(b, at(60, 336, 96, 40), tr("SUPPLY"), 16, kMuted, true);
    b.add(Pipe::kTypeId, at(164, 346, 400, 24))
        .with(Prop::Points, PointList{{0, 0.5}, {1, 0.5}})
        .with(Prop::Name, tr("Transfer line"));
    links.push_back({b.lastId(), Prop::Flowing, QStringLiteral("PumpRunning")});
    b.add(Pump::kTypeId, at(230, 318, 80, 80)).with(Prop::Name, tr("P-101"));
    links.push_back({b.lastId(), Prop::Running, QStringLiteral("PumpRunning")});
    label(b, at(230, 410, 100, 30), tr("P-101"), 18, kInk, true);
    b.add(Valve::kTypeId, at(380, 326, 64, 64)).with(Prop::Name, tr("V-101"));
    links.push_back({b.lastId(), Prop::Open, QStringLiteral("PumpRunning")});
    label(b, at(372, 410, 110, 30), tr("V-101"), 18, kInk, true);
    b.add(Tank::kTypeId, at(550, 258, 130, 210)).with(Prop::Name, tr("T-101"));
    links.push_back({b.lastId(), Prop::Value, QStringLiteral("Level")});
    links.push_back({b.lastId(), Prop::FillColor, QStringLiteral("Level"), QStringLiteral("ranges"),
                     ranges(90, QStringLiteral("#2763A5"), QStringLiteral("#B53238"))});
    label(b, at(560, 474, 130, 28), tr("T-101 LEVEL"), 16, kMuted, true);
    b.add(Bar::kTypeId, at(688, 266, 84, 194))
        .with(Prop::Orientation, EnumValue{1})
        .with(Prop::ShowScale, true);
    links.push_back({b.lastId(), Prop::Value, QStringLiteral("Level")});
    b.add(IoField::kTypeId, at(680, 470, 86, 36))
        .with(Prop::IoMode, EnumValue{IoField::Output})
        .with(Prop::Unit, QStringLiteral("%"));
    links.push_back({b.lastId(), Prop::Value, QStringLiteral("Level")});
    readout(b, links, 48, tr("Actual flow"), QStringLiteral("Flow"), QStringLiteral("L/min"), 1);
    readout(b, links, 292, tr("Line pressure"), QStringLiteral("Pressure"), QStringLiteral("bar"),
            2);
    readout(b, links, 536, tr("Product temperature"), QStringLiteral("Temperature"),
            QStringLiteral("°C"), 1);

    panel(b, at(808, 164, 448, 450));
    label(b, at(832, 180, 400, 32), tr("OPERATOR CONTROLS"), 18, kInk, true);
    button(b, events, at(832, 232, 188, 64), tr("START"), tr("Start transfer"), kGreen,
           {setTag(QStringLiteral("RunCommand"), QStringLiteral("true"))});
    button(b, events, at(1044, 232, 188, 64), tr("STOP"), tr("Stop transfer"), kRed,
           {setTag(QStringLiteral("RunCommand"), QStringLiteral("false"))});
    b.add(Switch::kTypeId, at(832, 326, 188, 52)).with(Prop::Name, tr("Automatic filling"));
    links.push_back({b.lastId(), Prop::Checked, QStringLiteral("AutoMode")});
    label(b, at(1044, 322, 188, 66), tr("AUTO fills 30-80%\nOFF: manual transfer"), 16, kMuted);
    button(b, events, at(832, 414, 188, 60), tr("HOLD TO JOG"), tr("Jog pump"), kBlue, {},
           {setTag(QStringLiteral("Jog"), QStringLiteral("true"))},
           {setTag(QStringLiteral("Jog"), QStringLiteral("false"))});
    label(b, at(1044, 408, 188, 76), tr("Hold: pump runs\nRelease: jog stops"), 16, kMuted);
    label(b, at(832, 502, 400, 38), tr("Stop clears the run command. Auto pauses at 80%."), 16,
          kMuted);
    label(b, at(832, 552, 400, 44), tr("Simulated process only - no PLC connection"), 16, kMuted);

    panel(b, at(24, 634, 1232, 142));
    label(b, at(48, 646, 500, 28), tr("FLOW SETPOINT  /  shared tag: FlowSetpoint"), 18, kInk,
          true);
    b.add(IoField::kTypeId, at(48, 690, 192, 60))
        .with(Prop::Name, tr("Flow setpoint"))
        .with(Prop::Unit, QStringLiteral("L/min"))
        .with(Prop::Decimals, 1);
    links.push_back({b.lastId(), Prop::Value, QStringLiteral("FlowSetpoint")});
    b.add(Slider::kTypeId, at(272, 690, 450, 64)).with(Prop::Name, tr("Adjust flow setpoint"));
    links.push_back({b.lastId(), Prop::Value, QStringLiteral("FlowSetpoint")});
    button(b, events, at(752, 690, 64, 60), tr("-5"), tr("Decrease flow"), kBlue,
           {incrementTag(QStringLiteral("FlowSetpoint"), QStringLiteral("-5"))});
    button(b, events, at(832, 690, 64, 60), tr("+5"), tr("Increase flow"), kBlue,
           {incrementTag(QStringLiteral("FlowSetpoint"), QStringLiteral("5"))});
    button(b, events, at(912, 690, 120, 60), tr("50 L/min"), tr("Preset flow"), kBlue,
           {setTag(QStringLiteral("FlowSetpoint"), QStringLiteral("50"))});
    b.add(Gauge::kTypeId, at(1080, 646, 136, 120)).with(Prop::Name, tr("Flow setpoint gauge"));
    links.push_back({b.lastId(), Prop::Value, QStringLiteral("FlowSetpoint")});
    label(b, at(24, 778, 1232, 22),
          tr("F5: restart simulation   |   Shift+F5: stop   |   Tap a value or drag the slider   | "
             "  Tab + Enter / arrow keys"),
          14, kMuted);

    auto inserted = project->insertScreen(b.build());
    Q_ASSERT(inserted);
    wire(**inserted, links, events);

    // A second screen to navigate to: the key values at a glance.
    links.clear();
    events.clear();
    ScreenBuilder o(overview);
    header(o, events, tr("LINE 03  /  OVERVIEW"), tr("Key values of the buffer filling station"),
           tr("FILLING >"), filling);
    panel(o, at(24, 120, 1232, 520));
    label(o, at(48, 140, 700, 32), tr("BUFFER FILLING STATION"), 18, kInk, true);
    o.add(Tank::kTypeId, at(80, 200, 160, 260)).with(Prop::Name, tr("T-101 overview"));
    links.push_back({o.lastId(), Prop::Value, QStringLiteral("Level")});
    o.add(Pump::kTypeId, at(320, 300, 100, 100)).with(Prop::Name, tr("P-101 overview"));
    links.push_back({o.lastId(), Prop::Running, QStringLiteral("PumpRunning")});
    readout(o, links, 480, tr("T-101 level"), QStringLiteral("Level"), QStringLiteral("%"), 1);
    readout(o, links, 720, tr("Actual flow"), QStringLiteral("Flow"), QStringLiteral("L/min"), 1);
    readout(o, links, 960, tr("Line pressure"), QStringLiteral("Pressure"), QStringLiteral("bar"),
            2);
    label(o, at(480, 200, 700, 80),
          tr("Operate the pump on the Filling screen.\nThe values here follow the same tags."), 18,
          kMuted);
    auto second = project->insertScreen(o.build());
    Q_ASSERT(second);
    wire(**second, links, events);

    // A popup: a smaller screen shown over the current one.
    links.clear();
    events.clear();
    ScreenBuilder d(details);
    d.size({720, 420}).background(QColor(Qt::white));
    d.add(TextBox::kTypeId, {24, 16, 520, 40})
        .with(Prop::Text, tr("P-101 TRANSFER PUMP"))
        .with(Prop::Font, fontOf(20, true))
        .with(Prop::TextColor, kInk);
    d.add(Pump::kTypeId, {40, 90, 140, 140}).with(Prop::Name, tr("P-101 popup"));
    links.push_back({d.lastId(), Prop::Running, QStringLiteral("PumpRunning")});
    const auto value =
        [&](double y, const QString& title, const QString& tag, const QString& unit, int decimals)
    {
        d.add(TextBox::kTypeId, {220, y, 220, 32})
            .with(Prop::Text, title)
            .with(Prop::Font, fontOf(15))
            .with(Prop::TextColor, kMuted);
        d.add(IoField::kTypeId, {450, y - 4, 230, 44})
            .with(Prop::Name, title)
            .with(Prop::Unit, unit)
            .with(Prop::Decimals, decimals)
            .with(Prop::IoMode, EnumValue{IoField::Output});
        links.push_back({d.lastId(), Prop::Value, tag});
    };
    value(96, tr("Actual flow"), QStringLiteral("Flow"), QStringLiteral("L/min"), 1);
    value(156, tr("Line pressure"), QStringLiteral("Pressure"), QStringLiteral("bar"), 2);
    value(216, tr("Flow setpoint"), QStringLiteral("FlowSetpoint"), QStringLiteral("L/min"), 1);
    button(d, events, {40, 300, 180, 70}, tr("START"), tr("Start from popup"), kGreen,
           {setTag(QStringLiteral("RunCommand"), QStringLiteral("true"))});
    button(d, events, {240, 300, 180, 70}, tr("STOP"), tr("Stop from popup"), kRed,
           {setTag(QStringLiteral("RunCommand"), QStringLiteral("false"))});
    button(d, events, {500, 300, 180, 70}, tr("CLOSE"), tr("Close popup"), kBlue, {closePopup()});
    auto popup = project->insertScreen(d.build());
    Q_ASSERT(popup);
    wire(**popup, links, events);
    const Tag tags[] = {
        tag(QStringLiteral("RunCommand"), Tag::Type::Bool, 0, 1, false, tr("Commands"),
            tr("Start/Stop request")),
        tag(QStringLiteral("AutoMode"), Tag::Type::Bool, 0, 1, true, tr("Commands"),
            tr("Automatic fill band 30-80%")),
        tag(QStringLiteral("Jog"), Tag::Type::Bool, 0, 1, false, tr("Commands"),
            tr("Momentary pump request")),
        tag(QStringLiteral("FlowSetpoint"), Tag::Type::Real, 0, 100, 50.0, tr("Commands"),
            tr("Shared flow setpoint"), QStringLiteral("L/min")),
        tag(QStringLiteral("HighLevel"), Tag::Type::Bool, 0, 1, false, tr("Measurements"),
            tr("High tank level interlock")),
        tag(QStringLiteral("Level"), Tag::Type::Real, 0, 100, 35.0, tr("Measurements"),
            tr("T-101 level"), QStringLiteral("%")),
        tag(QStringLiteral("PumpRunning"), Tag::Type::Bool, 0, 1, false, tr("Measurements"),
            tr("P-101 running feedback")),
        tag(QStringLiteral("Pressure"), Tag::Type::Real, 0, 6, 0.4, tr("Measurements"),
            tr("Line pressure"), QStringLiteral("bar")),
        tag(QStringLiteral("Flow"), Tag::Type::Real, 0, 100, 0.0, tr("Measurements"),
            tr("Actual feed flow"), QStringLiteral("L/min")),
        tag(QStringLiteral("Temperature"), Tag::Type::Real, 0, 50, 22.0, tr("Measurements"),
            tr("Product temperature"), QStringLiteral("°C")),
    };
    for (Tag t : tags)
    {
        // The operator sets the flow within the pump's 0..100 L/min.
        t.limitInput = t.name == u"FlowSetpoint";
        [[maybe_unused]] const auto added = project->tags().insert(t);
        Q_ASSERT(added);
    }
    return project;
}
} // namespace hmi
