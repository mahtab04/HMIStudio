#include "quick/RuntimeInputHandler.h"

#include "comm/TagEngine.h"
#include "core/model/HmiObject.h"
#include "core/model/Screen.h"
#include "core/property/DescriptorBuilders.h"
#include "core/property/Prop.h"

#include <QKeyEvent>
#include <QLocale>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

namespace hmi
{
namespace
{
using Kind = RuntimeBehavior::Kind;

struct Located
{
    const HmiObject* object;
    QTransform toScreen;
};

void locate(std::span<const std::unique_ptr<HmiObject>> objects, const QTransform& parent,
            std::vector<Located>& result)
{
    for (const auto& object : objects)
    {
        if (!object->isVisible())
            continue;
        const QTransform transform = object->sceneTransform() * parent;
        if (object->children().empty())
            result.push_back({object.get(), transform});
        else
            locate(object->children(), object->childTransform() * transform, result);
    }
}
} // namespace

RuntimeInputHandler::RuntimeInputHandler(TagEngine& engine, QObject* parent)
    : QObject(parent)
    , m_engine(engine)
{
}

RuntimeInputHandler::~RuntimeInputHandler()
{
    releaseMomentary();
}

void RuntimeInputHandler::setScreen(const Screen* screen)
{
    cancel();
    dismiss();
    m_focus = {};
    m_screen = screen;
    m_error.clear();
    Q_EMIT focusChanged();
    Q_EMIT inputChanged();
}

RuntimeInputHandler::Hit RuntimeInputHandler::hit(QPointF scene) const
{
    if (!m_screen || !QRectF(QPointF(), m_screen->size()).contains(scene))
        return {};
    std::vector<Located> objects;
    locate(m_screen->objects(), {}, objects);
    for (auto it = objects.rbegin(); it != objects.rend(); ++it)
    {
        bool invertible = false;
        const QTransform inverse = it->toScreen.inverted(&invertible);
        if (invertible && it->object->hitShape().contains(inverse.map(scene)))
            return {it->object, it->toScreen,
                    it->toScreen.mapRect(QRectF(QPointF(), it->object->geometry().size()))};
    }
    return {};
}

bool RuntimeInputHandler::reject(const QString& message)
{
    m_error = message;
    Q_EMIT inputChanged();
    return false;
}

bool RuntimeInputHandler::writeProperty(const HmiObject& object, PropertyId property,
                                        const PropertyValue& value)
{
    const Binding* binding = object.binding(property);
    if (!binding)
        return reject(tr("Bind this control's process value to a tag in Properties."));
    auto converted = binding->transform->inverse(value);
    if (!converted)
        return reject(converted.error().message);
    const auto result = m_engine.writeChecked(binding->tag, *converted);
    if (!result)
        return reject(result.error().message);
    m_error.clear();
    Q_EMIT inputChanged();
    return true;
}

PropertyValue RuntimeInputHandler::currentValue(const HmiObject& object, PropertyId property) const
{
    const auto* binding = object.binding(property);
    const auto* descriptor = findDescriptor(object.descriptors(), property);
    if (binding && descriptor)
    {
        const auto result = binding->transform->apply(m_engine.value(binding->tag), *descriptor);
        if (result)
            return *result;
    }
    return *object.property(property);
}

bool RuntimeInputHandler::action(const HmiObject& object, bool pressed)
{
    const TagAction operation = static_cast<TagAction>(object.value<EnumValue>(Prop::Action).value);
    if (operation == TagAction::None)
        return pressed ? false : reject(tr("Choose a tag operation and target tag in Properties."));
    if ((operation == TagAction::Momentary) != pressed)
        return false;
    const QString name = object.value<TagRef>(Prop::ActionTag).tagName;
    const Tag* tag = m_engine.definition(name);
    if (!tag)
        return reject(tr("Target tag '%1' is missing.").arg(name));
    TagValue value = object.value<QString>(Prop::ActionValue);
    if (operation == TagAction::Toggle || operation == TagAction::Momentary)
    {
        if (tag->type != Tag::Type::Bool)
            return reject(tr("Toggle and momentary operations need a Boolean tag."));
        value = operation == TagAction::Momentary || !TagValues::toBool(m_engine.value(name));
    }
    if (operation == TagAction::Increment)
    {
        if (tag->type != Tag::Type::Int && tag->type != Tag::Type::Real)
            return reject(tr("Increment needs an integer or real tag."));
        const QString stepText = object.value<QString>(Prop::ActionValue).trimmed();
        if (tag->type == Tag::Type::Int)
        {
            bool ok = false;
            const qint64 step = stepText.toLongLong(&ok);
            const auto current = m_engine.value(name);
            const auto* integer = std::get_if<qint64>(&current);
            if (!ok || !integer ||
                (step > 0 && *integer > std::numeric_limits<qint64>::max() - step) ||
                (step < 0 && *integer < std::numeric_limits<qint64>::min() - step))
                return reject(tr("Enter a valid integer step within the supported range."));
            value = *integer + step;
        }
        else
        {
            bool ok = false;
            const double step = stepText.toDouble(&ok);
            const auto current = TagValues::toNumber(m_engine.value(name));
            if (!ok || !current || !std::isfinite(step))
                return reject(tr("Enter a finite numeric step."));
            value = *current + step;
        }
    }
    const auto result = m_engine.writeChecked(name, value);
    if (!result)
        return reject(result.error().message);
    if (operation == TagAction::Momentary)
        m_momentary = name;
    m_error.clear();
    Q_EMIT inputChanged();
    return true;
}

void RuntimeInputHandler::releaseMomentary()
{
    if (m_momentary.isEmpty())
        return;
    const QString tag = std::exchange(m_momentary, {});
    const auto result = m_engine.writeChecked(tag, false);
    if (!result && m_engine.isRunning())
        reject(result.error().message);
}

void RuntimeInputHandler::cancel()
{
    releaseMomentary();
    m_pressed = {};
    m_drag = {};
    m_keyboardActive = false;
}

void RuntimeInputHandler::slider(const Hit& target, QPointF scene)
{
    if (!target.object)
        return;
    if (const auto value = target.object->runtimeValueAt(target.toScreen.inverted().map(scene)))
        writeProperty(*target.object, target.object->runtimeBehavior().property, *value);
}

void RuntimeInputHandler::handleGesture(const PointerGesture& gesture)
{
    using G = PointerGesture::Kind;
    if (!m_screen || isOpen())
        return;
    switch (gesture.kind)
    {
    case G::Press:
        cancel();
        m_pressed = hit(gesture.scenePos);
        m_focus = m_pressed;
        Q_EMIT focusChanged();
        if (m_pressed.object && m_pressed.object->runtimeBehavior().kind == Kind::Action)
            action(*m_pressed.object, true);
        break;
    case G::Release:
        releaseMomentary();
        break;
    case G::Tap:
    case G::DoubleTap:
    {
        const Hit target = hit(gesture.scenePos);
        if (target.object && target.object == m_pressed.object)
            activate(target, gesture.scenePos);
        m_pressed = {};
        break;
    }
    case G::DragBegin:
        if (m_pressed.object && m_pressed.object->runtimeBehavior().kind == Kind::Slider)
            m_drag = m_pressed;
        slider(m_drag, gesture.scenePos);
        break;
    case G::DragUpdate:
        slider(m_drag, gesture.scenePos);
        break;
    case G::DragEnd:
        slider(m_drag, gesture.scenePos);
        cancel();
        break;
    case G::Cancel:
    case G::DragCancel:
    case G::PinchBegin:
        cancel();
        break;
    default:
        break;
    }
}

void RuntimeInputHandler::activate(const Hit& target, QPointF scene)
{
    if (!target.object)
        return;
    const HmiObject& object = *target.object;
    const auto behavior = object.runtimeBehavior();
    switch (behavior.kind)
    {
    case Kind::ValueInput:
    case Kind::Choice:
        beginInput(object);
        break;
    case Kind::Toggle:
        if (const auto value = currentValue(object, behavior.property);
            const bool* flag = std::get_if<bool>(&value))
            writeProperty(object, behavior.property, !*flag);
        break;
    case Kind::RowChoice:
        if (auto value = object.runtimeValueAt(target.toScreen.inverted().map(scene)))
            writeProperty(object, behavior.property, *value);
        break;
    case Kind::Slider:
        slider(target, scene);
        break;
    case Kind::Action:
        action(object, false);
        break;
    case Kind::None:
        break;
    }
}

void RuntimeInputHandler::beginInput(const HmiObject& object)
{
    const auto behavior = object.runtimeBehavior();
    const auto* binding = object.binding(behavior.property);
    if (!binding)
    {
        reject(tr("Bind this control's process value to a tag in Properties."));
        return;
    }
    if (m_engine.isReadOnly())
    {
        reject(tr("PLC monitoring is read only. Use Simulation to operate controls."));
        return;
    }
    m_input = &object;
    m_title = object.name().isEmpty() ? binding->tag : object.name();
    m_choices.clear();
    m_selected = -1;
    if (behavior.kind == Kind::Choice)
    {
        m_choices = Describe::splitItems(object.value<QString>(Prop::Items));
        const auto current = currentValue(object, behavior.property);
        const int* index = std::get_if<int>(&current);
        m_selected = index ? *index : -1;
        if (m_choices.isEmpty())
        {
            dismiss();
            reject(tr("Configure the control's entries in Properties."));
            return;
        }
    }
    else
    {
        const auto value = currentValue(object, behavior.property);
        const auto number = std::get_if<double>(&value);
        m_text = number ? QLocale().toString(*number, 'g', 10) : QString();
        m_replaceInput = true;
    }
    m_error.clear();
    Q_EMIT inputChanged();
}

void RuntimeInputHandler::dismiss()
{
    m_input = nullptr;
    m_choices.clear();
    Q_EMIT inputChanged();
}

void RuntimeInputHandler::setText(const QString& text)
{
    if (m_text == text)
        return;
    m_text = text;
    m_replaceInput = false;
    m_error.clear();
    Q_EMIT inputChanged();
}

void RuntimeInputHandler::inputKey(const QString& key)
{
    if (std::exchange(m_replaceInput, false) && key != u"sign" && key != u"backspace")
        m_text.clear();
    if (key == u"backspace")
        setText(m_text.left(std::max(0, int(m_text.size()) - 1)));
    else if (key == u"clear")
        setText({});
    else if (key == u"sign")
        setText(m_text.startsWith(u'-') ? m_text.mid(1) : u'-' + m_text);
    else
        setText(m_text + (key == u"decimal" ? QLocale().decimalPoint() : key));
}

bool RuntimeInputHandler::submit()
{
    if (!m_input)
        return false;
    bool ok = false;
    double number = QLocale().toDouble(m_text.trimmed(), &ok);
    if (!ok)
        number = m_text.trimmed().toDouble(&ok);
    if (!ok || !std::isfinite(number))
        return reject(tr("Enter a finite numeric value."));
    const auto property = m_input->runtimeBehavior().property;
    const auto* binding = m_input->binding(property);
    const Tag* tag = binding ? m_engine.definition(binding->tag) : nullptr;
    if (tag && tag->type == Tag::Type::Int && binding->transform->kind() == u"direct" &&
        number != std::trunc(number))
        return reject(tr("This tag requires a whole number."));
    if (!writeProperty(*m_input, property, number))
        return false;
    dismiss();
    return true;
}

bool RuntimeInputHandler::choose(int index)
{
    if (!m_input || index < 0 || index >= m_choices.size())
        return false;
    if (!writeProperty(*m_input, m_input->runtimeBehavior().property, index))
        return false;
    dismiss();
    return true;
}

void RuntimeInputHandler::keyInput(int key, Qt::KeyboardModifiers modifiers, bool released)
{
    if (!m_screen || isOpen())
        return;
    if (key == Qt::Key_Escape)
    {
        cancel();
        return;
    }
    if (!released && (key == Qt::Key_Tab || key == Qt::Key_Backtab))
    {
        cancel();
        std::vector<Located> objects;
        locate(m_screen->objects(), {}, objects);
        std::erase_if(objects, [](const Located& item)
                      { return item.object->runtimeBehavior().kind == Kind::None; });
        if (objects.empty())
            return;
        auto it = std::ranges::find_if(objects, [this](const Located& item)
                                       { return item.object == m_focus.object; });
        const int count = int(objects.size());
        const int current = it == objects.end() ? -1 : int(it - objects.begin());
        const bool backwards = key == Qt::Key_Backtab || modifiers.testFlag(Qt::ShiftModifier);
        const int index = current < 0 ? (backwards ? count - 1 : 0)
                                      : (current + (backwards ? count - 1 : 1)) % count;
        const auto& next = objects[std::size_t(index)];
        m_focus = {next.object, next.toScreen,
                   next.toScreen.mapRect(QRectF(QPointF(), next.object->geometry().size()))};
        Q_EMIT focusChanged();
        return;
    }
    if (!m_focus.object)
        return;
    const auto behavior = m_focus.object->runtimeBehavior();
    if (key == Qt::Key_Space || key == Qt::Key_Return || key == Qt::Key_Enter)
    {
        if (!released)
        {
            m_keyboardActive = true;
            if (behavior.kind == Kind::Action)
                action(*m_focus.object, true);
        }
        if (released && std::exchange(m_keyboardActive, false))
        {
            releaseMomentary();
            activate(m_focus, m_focus.bounds.center());
        }
    }
    if (!released && behavior.kind == Kind::Slider &&
        (key == Qt::Key_Left || key == Qt::Key_Right || key == Qt::Key_Up || key == Qt::Key_Down))
    {
        const HmiObject& object = *m_focus.object;
        const double low = object.value<double>(Prop::Minimum);
        const double high = object.value<double>(Prop::Maximum);
        const auto* binding = object.binding(behavior.property);
        const auto* tag = binding ? m_engine.definition(binding->tag) : nullptr;
        const double step =
            std::max(tag && tag->type == Tag::Type::Int ? 1.0 : 0.01, (high - low) / 100.0);
        const double direction = key == Qt::Key_Left || key == Qt::Key_Down ? -1.0 : 1.0;
        const auto current = currentValue(object, behavior.property);
        if (const double* number = std::get_if<double>(&current); number && low <= high)
            writeProperty(object, behavior.property,
                          std::clamp(*number + direction * step, low, high));
    }
    if (!released && behavior.kind == Kind::RowChoice && (key == Qt::Key_Up || key == Qt::Key_Down))
    {
        const HmiObject& object = *m_focus.object;
        const int count = int(Describe::splitItems(object.value<QString>(Prop::Items)).size());
        const auto current = currentValue(object, behavior.property);
        if (const int* index = std::get_if<int>(&current); index && count > 0)
            writeProperty(object, behavior.property,
                          std::clamp(*index + (key == Qt::Key_Up ? -1 : 1), 0, count - 1));
    }
}
} // namespace hmi
