#pragma once

#include "core/model/Events.h"
#include "core/model/Ids.h"
#include "core/property/PropertyValue.h"
#include "quick/input/PointerGesture.h"

#include <QObject>
#include <QRectF>
#include <QStringList>
#include <QTransform>
#include <qqmlintegration.h>

namespace hmi
{
class HmiObject;
class IAlarmSource;
class Screen;
class TagEngine;

// Operates a play screen exclusively through TagEngine. The editor document
// is never touched. Object types provide input semantics and local hit values.
class RuntimeInputHandler : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by the simulation controller")
    Q_PROPERTY(bool open READ isOpen NOTIFY inputChanged)
    Q_PROPERTY(QString title READ title NOTIFY inputChanged)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY inputChanged)
    Q_PROPERTY(QStringList choices READ choices NOTIFY inputChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY inputChanged)
    Q_PROPERTY(QString error READ error NOTIFY inputChanged)
    Q_PROPERTY(QRectF focusBounds READ focusBounds NOTIFY focusChanged)

public:
    explicit RuntimeInputHandler(TagEngine& engine, QObject* parent = nullptr);
    ~RuntimeInputHandler() override;
    void setScreen(const Screen* screen);
    // The running alarms, operated through alarm controls.
    void setAlarms(IAlarmSource* alarms) { m_alarms = alarms; }
    void handleGesture(const PointerGesture& gesture);
    void keyInput(int key, Qt::KeyboardModifiers modifiers, bool released);
    void cancel();
    Q_INVOKABLE void dismiss();
    Q_INVOKABLE bool submit();
    Q_INVOKABLE bool choose(int index);
    Q_INVOKABLE void inputKey(const QString& key);
    Q_INVOKABLE void setText(const QString& text);
    [[nodiscard]] bool isOpen() const noexcept { return m_input != nullptr; }
    [[nodiscard]] QString title() const { return m_title; }
    [[nodiscard]] QString text() const { return m_text; }
    [[nodiscard]] QStringList choices() const { return m_choices; }
    [[nodiscard]] int selectedIndex() const noexcept { return m_selected; }
    [[nodiscard]] QString error() const { return m_error; }
    [[nodiscard]] QRectF focusBounds() const { return m_focus.bounds; }

Q_SIGNALS:
    void inputChanged();
    void focusChanged();
    // An Open screen action: the controller shows that screen.
    void screenRequested(const QString& screen);
    // Show popup and Close popup actions.
    void popupRequested(const QString& screen);
    void popupCloseRequested();

private:
    struct Hit
    {
        const HmiObject* object = nullptr;
        QTransform toScreen;
        QRectF bounds;
    };
    [[nodiscard]] Hit hit(QPointF scene) const;
    void activate(const Hit& hit, QPointF scene);
    void beginInput(const HmiObject& object);
    bool writeProperty(const HmiObject& object, PropertyId property, const PropertyValue& value);
    // Runs an event's actions in order; a failing action is reported and the
    // others still run.
    void runActions(ActionList actions);
    bool runAction(const Action& action);
    void press(const HmiObject& object);
    void releaseHeld();
    void slider(const Hit& hit, QPointF scene);
    bool reject(const QString& message);
    [[nodiscard]] PropertyValue currentValue(const HmiObject& object, PropertyId property) const;

    TagEngine& m_engine;
    const Screen* m_screen = nullptr;
    Hit m_pressed;
    Hit m_drag;
    Hit m_focus;
    const HmiObject* m_input = nullptr;
    QString m_title;
    QString m_text;
    QStringList m_choices;
    int m_selected = -1;
    QString m_error;
    IAlarmSource* m_alarms = nullptr;
    const HmiObject* m_held = nullptr; // pressed: its Release actions are due
    bool m_keyboardActive = false;
    bool m_replaceInput = false;
};
} // namespace hmi
