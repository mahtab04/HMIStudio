#pragma once

#include "core/model/Ids.h"
#include "core/tags/Binding.h"

#include <QHash>
#include <QObject>
#include <QStringList>

#include <memory>
#include <vector>

namespace hmi
{

class HmiObject;
class Project;
class Screen;
class TagEngine;

// Drives a screen from tag values while simulating (and in the runtime,
// phase 9). It works on a play copy of the screen in a project of its own,
// so the document is never changed and the undo stack never sees a value:
// the canvas shows the copy, whose notifier reports each driven property.
// Bindings include nested group children; their top-level item is refreshed.
class BindingEvaluator : public QObject
{
    Q_OBJECT

public:
    explicit BindingEvaluator(TagEngine& engine, QObject* parent = nullptr);
    ~BindingEvaluator() override;
    BindingEvaluator(const BindingEvaluator&) = delete;
    BindingEvaluator& operator=(const BindingEvaluator&) = delete;
    BindingEvaluator(BindingEvaluator&&) = delete;
    BindingEvaluator& operator=(BindingEvaluator&&) = delete;

    // Makes a play copy of `screen` and applies every binding with the
    // current tag values.
    void show(const Screen& screen);
    void clear();

    [[nodiscard]] const Screen* playScreen() const;
    [[nodiscard]] Project& playProject() const noexcept { return *m_play; }

    // Bindings that could not be applied the last time, as "object:
    // reason" texts, for the output panel.
    [[nodiscard]] const QStringList& problems() const noexcept { return m_problems; }
    // Adds a problem found elsewhere in play mode (an action that failed).
    void report(const QString& problem);

Q_SIGNALS:
    void problemsChanged();

private:
    struct Target
    {
        HmiObject* object = nullptr;
        ObjectId root;
        Binding binding;
    };

    void apply(const QStringList& tags);

    TagEngine& m_engine;
    std::unique_ptr<Project> m_play;
    QHash<QString, std::vector<Target>> m_byTag; // lower-case tag name
    QStringList m_problems;
};

} // namespace hmi
