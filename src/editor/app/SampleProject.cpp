#include "editor/app/SampleProject.h"

#include "core/model/Project.h"
#include "core/model/ScreenBuilder.h"
#include "core/objects/Ellipse.h"
#include "core/objects/Line.h"
#include "core/objects/Rectangle.h"
#include "core/objects/TextBox.h"
#include "core/property/Prop.h"

#include <QCoreApplication>

namespace hmi
{

namespace
{

QFont fontOf(int pixelSize, bool bold = false)
{
    QFont f;
    f.setPixelSize(pixelSize);
    f.setBold(bold);
    return f;
}

void valueCard(ScreenBuilder& b, double x, double y, const QString& label, const QString& value)
{
    b.add(Rectangle::kTypeId, {x, y, 230, 116})
        .with(Prop::FillColor, QColor(0xF4, 0xF5, 0xF7))
        .with(Prop::LineColor, QColor(0xC9, 0xCE, 0xD6))
        .with(Prop::CornerRadius, 6.0);
    b.add(TextBox::kTypeId, {x + 20, y + 14, 190, 24})
        .with(Prop::Text, label)
        .with(Prop::TextColor, QColor(0x4A, 0x50, 0x58));
    b.add(TextBox::kTypeId, {x + 20, y + 44, 190, 52})
        .with(Prop::Text, value)
        .with(Prop::Font, fontOf(36))
        .with(Prop::TextColor, QColor(0x1F, 0x23, 0x28));
}

} // namespace

std::unique_ptr<Project> makeSampleProject()
{
    const auto tr = [](const char* text)
    {
        return QCoreApplication::translate("hmi::SampleProject", text);
    };
    auto project = std::make_unique<Project>(tr("Line 3 · Filling Station"));

    ScreenBuilder b(tr("02 Filling"));
    b.size({1280, 800}).background(QColor(0xE6, 0xE8, 0xEB));

    // Header
    b.add(Rectangle::kTypeId, {0, 0, 1280, 68})
        .with(Prop::FillColor, QColor(0x2B, 0x2F, 0x36))
        .with(Prop::LineWidth, 0.0)
        .with(Prop::Name, tr("Header"));
    b.add(TextBox::kTypeId, {28, 0, 600, 68})
        .with(Prop::Text, tr("Filling Station · Line 3"))
        .with(Prop::Font, fontOf(22, true))
        .with(Prop::TextColor, QColor(Qt::white));

    // Tank T-101 with its level
    b.add(Rectangle::kTypeId, {110, 140, 180, 300})
        .with(Prop::FillColor, QColor(0xEE, 0xF0, 0xF2))
        .with(Prop::LineColor, QColor(0x2B, 0x2F, 0x36))
        .with(Prop::LineWidth, 3.0)
        .with(Prop::CornerRadius, 22.0)
        .with(Prop::Name, tr("T-101"));
    b.add(Rectangle::kTypeId, {113, 240, 174, 197})
        .with(Prop::FillColor, QColor(0x3B, 0x6F, 0xD4))
        .with(Prop::LineWidth, 0.0)
        .with(Prop::CornerRadius, 19.0)
        .with(Prop::Name, tr("T-101 level"));
    b.add(TextBox::kTypeId, {110, 170, 180, 40})
        .with(Prop::Text, tr("T-101"))
        .with(Prop::Font, fontOf(22, true))
        .with(Prop::HorizontalAlignment, EnumValue{TextBox::AlignHCenter});
    b.add(TextBox::kTypeId, {110, 310, 180, 56})
        .with(Prop::Text, tr("68 %"))
        .with(Prop::Font, fontOf(36, true))
        .with(Prop::TextColor, QColor(Qt::white))
        .with(Prop::HorizontalAlignment, EnumValue{TextBox::AlignHCenter});

    // Pipe, pump and filler
    b.add(Line::kTypeId, {200, 440, 0, 140})
        .with(Prop::LineWidth, 8.0)
        .with(Prop::LineColor, QColor(0x6B, 0x72, 0x7C));
    b.add(Line::kTypeId, {200, 580, 820, 0})
        .with(Prop::LineWidth, 8.0)
        .with(Prop::LineColor, QColor(0x6B, 0x72, 0x7C));
    b.add(Ellipse::kTypeId, {460, 540, 80, 80})
        .with(Prop::FillColor, QColor(Qt::white))
        .with(Prop::LineWidth, 4.0)
        .with(Prop::Name, tr("P-101"));
    b.add(TextBox::kTypeId, {430, 630, 140, 28})
        .with(Prop::Text, tr("P-101 Running"))
        .with(Prop::HorizontalAlignment, EnumValue{TextBox::AlignHCenter});
    b.add(Rectangle::kTypeId, {1000, 530, 230, 100})
        .with(Prop::FillColor, QColor(0xF4, 0xF5, 0xF7))
        .with(Prop::LineWidth, 3.0)
        .with(Prop::CornerRadius, 6.0)
        .with(Prop::Name, tr("F-201"));
    b.add(TextBox::kTypeId, {1020, 545, 200, 70})
        .with(Prop::Text, tr("F-201 Filler\n118 bpm"))
        .with(Prop::Font, fontOf(20));

    valueCard(b, 360, 120, tr("Pressure"), tr("2.4 bar"));
    valueCard(b, 360, 256, tr("Flow"), tr("42.5 L/min"));
    valueCard(b, 360, 392, tr("Temperature"), tr("21.8 °C"));

    // Warning banner
    b.add(Rectangle::kTypeId, {0, 720, 1280, 80})
        .with(Prop::FillColor, QColor(0xF2, 0xA1, 0x3A))
        .with(Prop::LineWidth, 0.0)
        .with(Prop::Name, tr("Alarm banner"));
    b.add(TextBox::kTypeId, {28, 720, 900, 80})
        .with(Prop::Text, tr("Warning · T-101 level approaching high limit"))
        .with(Prop::Font, fontOf(20, true));

    [[maybe_unused]] auto inserted = project->insertScreen(b.build());
    Q_ASSERT(inserted);
    return project;
}

} // namespace hmi
