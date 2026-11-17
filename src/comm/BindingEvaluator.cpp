#include "comm/BindingEvaluator.h"

#include "comm/TagEngine.h"
#include "core/model/HmiObject.h"
#include "core/model/Project.h"

#include <functional>

namespace hmi
{

BindingEvaluator::BindingEvaluator(TagEngine& engine, QObject* parent)
    : QObject(parent)
    , m_engine(engine)
    , m_play(std::make_unique<Project>())
{
    connect(&m_engine, &TagEngine::valuesChanged, this, &BindingEvaluator::apply);
}

BindingEvaluator::~BindingEvaluator() = default;

void BindingEvaluator::clear()
{
    m_byTag.clear();
    m_play = std::make_unique<Project>();
    if (!m_problems.isEmpty())
    {
        m_problems.clear();
        Q_EMIT problemsChanged();
    }
}

void BindingEvaluator::show(const Screen& screen, const IScreenSource* screens)
{
    m_byTag.clear();
    // A fresh project for each screen, so items of the last one are gone.
    auto play = std::make_unique<Project>();
    auto inserted = play->insertScreen(screen.clone());
    Q_ASSERT(inserted);
    // Screen windows become live copies of the screens they show.
    if (screens)
    {
        std::vector<ObjectId> ids;
        for (const auto& object : (*inserted)->objects())
            ids.push_back(object->id());
        for (const ObjectId& id : ids)
        {
            Screen& copy = **inserted;
            auto stand = copy.object(id)->expandForRuntime(*screens, 0);
            if (!stand)
                continue;
            const auto index = copy.indexOf(id);
            (void)copy.takeObject(id);
            [[maybe_unused]] const auto placed = copy.insertObject(std::move(stand), index);
            Q_ASSERT(placed);
        }
    }
    m_play = std::move(play);
    QStringList tags;
    const std::function<void(HmiObject&, ObjectId)> collect = [&](HmiObject& object, ObjectId root)
    {
        for (const Binding& binding : object.bindings())
        {
            m_byTag[binding.tag.toLower()].push_back({&object, root, binding});
            tags << binding.tag;
        }
        for (const auto& child : object.children())
            collect(*child, root);
    };
    for (const auto& object : (*inserted)->objects())
        collect(*object, object->id());
    m_problems.clear();
    apply(tags);
    Q_EMIT problemsChanged();
}

const Screen* BindingEvaluator::playScreen() const
{
    return m_play->screenCount() ? m_play->screens().front().get() : nullptr;
}

void BindingEvaluator::apply(const QStringList& tags)
{
    Screen* screen = m_play->screenCount() ? m_play->screens().front().get() : nullptr;
    if (!screen)
        return;
    bool problemsChanged = false;
    for (const QString& tag : tags)
    {
        const auto it = m_byTag.constFind(tag.toLower());
        if (it == m_byTag.cend())
            continue;
        const TagValue value = m_engine.value(tag);
        if (std::holds_alternative<std::monostate>(value))
            continue; // unknown tag: the property keeps its designed value
        for (const Target& target : *it)
        {
            HmiObject* object = target.object;
            const PropertyDescriptor* d =
                object ? findDescriptor(object->descriptors(), target.binding.property) : nullptr;
            if (!d)
                continue;
            auto result = target.binding.transform->apply(value, *d);
            if (result)
                result = object->setProperty(d->id, *result).map([&] { return *result; });
            if (result && object->id() != target.root)
                Q_EMIT m_play->notifier().propertyChanged(target.root, d->id);
            if (!result)
            {
                const QString problem =
                    QStringLiteral("%1.%2: %3")
                        .arg(object->name().isEmpty() ? object->id().toString() : object->name(),
                             QLatin1StringView(d->key), result.error().message);
                if (!m_problems.contains(problem))
                {
                    m_problems << problem;
                    problemsChanged = true;
                }
            }
        }
    }
    if (problemsChanged)
        Q_EMIT this->problemsChanged();
}

void BindingEvaluator::report(const QString& problem)
{
    if (m_problems.contains(problem))
        return;
    m_problems << problem;
    Q_EMIT problemsChanged();
}

} // namespace hmi
