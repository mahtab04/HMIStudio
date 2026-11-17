#include "comm/BindingEvaluator.h"
#include "comm/TagEngine.h"
#include "core/model/GroupObject.h"
#include "core/model/Project.h"
#include "core/model/ScreenBuilder.h"
#include "core/objects/Button.h"
#include "core/objects/CheckBox.h"
#include "core/objects/Gauge.h"
#include "core/objects/IoField.h"
#include "core/objects/ListBox.h"
#include "core/objects/RadioGroup.h"
#include "core/objects/Rectangle.h"
#include "core/objects/Slider.h"
#include "core/objects/Switch.h"
#include "core/objects/SymbolicIoField.h"
#include "core/objects/TouchArea.h"
#include "core/property/Prop.h"
#include "quick/RuntimeInputHandler.h"
#include "quick/RuntimeView.h"
#include "quick/ScreenItem.h"

#include <QPointingDevice>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>

using namespace hmi;
using namespace Qt::StringLiterals;

class RuntimeInputTest : public QObject
{
    Q_OBJECT
private:
    TagEngine m_engine;
    BindingEvaluator m_evaluator{m_engine};
    RuntimeInputHandler m_input{m_engine};
    std::unique_ptr<Screen> m_document;
    std::unique_ptr<QQuickWindow> m_window;
    RuntimeView* m_view = nullptr;
    QPointingDevice* m_touch = nullptr;
    ObjectId m_io, m_slider, m_gauge, m_switch, m_choice;

    QPoint point(QPointF scene) const { return m_view->screenItem()->mapToScene(scene).toPoint(); }
    void tap(QPointF scene, bool touch)
    {
        if (touch)
        {
            QTest::touchEvent(m_window.get(), m_touch).press(0, point(scene), m_window.get());
            QTest::touchEvent(m_window.get(), m_touch).release(0, point(scene), m_window.get());
            QCoreApplication::processEvents();
        }
        else
            QTest::mouseClick(m_window.get(), Qt::LeftButton, {}, point(scene));
    }
    void devices()
    {
        QTest::addColumn<bool>("touch");
        QTest::newRow("mouse") << false;
        QTest::newRow("touch") << true;
    }
    const HmiObject* play(ObjectId id) const { return m_evaluator.playScreen()->object(id); }

private Q_SLOTS:
    void initTestCase() { m_touch = QTest::createTouchDevice(); }
    void init()
    {
        ScreenBuilder b(u"Operator"_s);
        b.size({720, 520});
        b.add(IoField::kTypeId, {10, 10, 120, 50});
        m_io = b.lastId();
        b.add(Slider::kTypeId, {160, 10, 200, 60});
        m_slider = b.lastId();
        b.add(Gauge::kTypeId, {400, 10, 100, 100});
        m_gauge = b.lastId();
        b.add(IoField::kTypeId, {10, 100, 120, 50}).with(Prop::IoMode, EnumValue{IoField::Output});
        b.add(Switch::kTypeId, {10, 160, 140, 50});
        m_switch = b.lastId();
        b.add(CheckBox::kTypeId, {170, 160, 140, 50});
        const auto check = b.lastId();
        b.add(ListBox::kTypeId, {10, 230, 150, 110}).with(Prop::Items, u"A;B;C"_s);
        const auto list = b.lastId();
        b.add(RadioGroup::kTypeId, {200, 230, 150, 110}).with(Prop::Items, u"A;B;C"_s);
        const auto radio = b.lastId();
        b.add(SymbolicIoField::kTypeId, {400, 230, 150, 50}).with(Prop::Items, u"A;B;C"_s);
        m_choice = b.lastId();
        // Buttons and a touch area operate tags through their events.
        b.add(Button::kTypeId, {10, 400, 110, 60});
        const auto setButton = b.lastId();
        b.add(Button::kTypeId, {140, 400, 110, 60});
        const auto toggleButton = b.lastId();
        b.add(Button::kTypeId, {270, 400, 110, 60});
        const auto incrementButton = b.lastId();
        b.add(Button::kTypeId, {400, 400, 110, 60});
        const auto holdButton = b.lastId();
        b.add(TouchArea::kTypeId, {530, 400, 110, 60});
        const auto touchArea = b.lastId();
        b.add(Button::kTypeId, {660, 400, 110, 60});
        const auto navigate = b.lastId();
        b.add(Rectangle::kTypeId, {10, 480, 100, 40});
        const auto plainShape = b.lastId();
        m_document = b.build();
        const auto tagAction = [](Action::Kind kind, const QString& tag, const QString& value = {})
        {
            return Action{.kind = kind, .tag = tag, .value = value, .screen = {}};
        };
        m_document->object(setButton)->setActions(
            EventKind::Click, {tagAction(Action::Kind::SetTag, u"Setpoint"_s, u"20"_s)});
        m_document->object(toggleButton)
            ->setActions(EventKind::Click, {tagAction(Action::Kind::ToggleTag, u"Flag"_s)});
        m_document->object(incrementButton)
            ->setActions(EventKind::Click,
                         {tagAction(Action::Kind::IncrementTag, u"Setpoint"_s, u"5"_s)});
        m_document->object(holdButton)
            ->setActions(EventKind::Press, {tagAction(Action::Kind::SetTag, u"Flag"_s, u"true"_s)});
        m_document->object(holdButton)
            ->setActions(EventKind::Release,
                         {tagAction(Action::Kind::SetTag, u"Flag"_s, u"false"_s)});
        m_document->object(touchArea)->setActions(
            EventKind::Click, {tagAction(Action::Kind::SetTag, u"Setpoint"_s, u"80"_s)});
        // Several actions, in order: a tag, then another screen.
        m_document->object(navigate)->setActions(
            EventKind::Click,
            {tagAction(Action::Kind::SetTag, u"Setpoint"_s, u"33"_s),
             Action{
                 .kind = Action::Kind::OpenScreen, .tag = {}, .value = {}, .screen = u"Next"_s}});
        // Any object can have events, not only buttons.
        m_document->object(plainShape)
            ->setActions(EventKind::Click, {tagAction(Action::Kind::ToggleTag, u"Flag"_s)});
        for (const auto id : {m_io, m_slider, m_gauge})
            QVERIFY(m_document->object(id)->setBinding(
                {Prop::Value, u"Setpoint"_s, Binding::direct()}));
        for (const auto id : {m_switch, check})
            QVERIFY(
                m_document->object(id)->setBinding({Prop::Checked, u"Flag"_s, Binding::direct()}));
        for (const auto id : {list, radio, m_choice})
            QVERIFY(m_document->object(id)->setBinding(
                {Prop::SelectedIndex, u"Choice"_s, Binding::direct()}));
        Tag number;
        number.name = u"Setpoint"_s;
        number.type = Tag::Type::Int;
        number.initial = qint64(30);
        number.limitInput = true; // operator input stays within 0..100
        Tag flag;
        flag.name = u"Flag"_s;
        flag.type = Tag::Type::Bool;
        Tag choice;
        choice.name = u"Choice"_s;
        choice.type = Tag::Type::Int;
        choice.maximum = 2;
        m_engine.start(std::vector<Tag>{number, flag, choice});
        m_evaluator.show(*m_document);
        m_input.setScreen(m_evaluator.playScreen());
        m_window = std::make_unique<QQuickWindow>();
        m_window->resize(800, 600);
        m_view = new RuntimeView(m_window->contentItem());
        m_view->setPosition({40, 30});
        m_view->setSize({720, 520});
        m_view->setScreen(m_evaluator.playScreen(), &m_evaluator.playProject().notifier());
        connect(m_view, &RuntimeView::gesture, &m_input, &RuntimeInputHandler::handleGesture);
        connect(m_view, &RuntimeView::inputCancelled, &m_input, &RuntimeInputHandler::cancel);
        connect(m_view, &RuntimeView::keyInput, &m_input, &RuntimeInputHandler::keyInput);
        m_window->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_window.get()));
        m_view->forceActiveFocus();
    }
    void cleanup()
    {
        m_input.setScreen(nullptr);
        m_window.reset();
        m_evaluator.clear();
        m_engine.stop();
        m_document.reset();
    }
    void sharedValue_data() { devices(); }
    void sharedValue()
    {
        QFETCH(bool, touch);
        tap({50, 35}, touch);
        QTRY_VERIFY(m_input.isOpen());
        m_input.setText(u"32"_s);
        QVERIFY(m_input.submit());
        QCOMPARE(m_engine.value(u"Setpoint"_s), TagValue(qint64(32)));
        for (auto id : {m_io, m_slider, m_gauge})
            QTRY_COMPARE(play(id)->value<double>(Prop::Value), 32.0);
        QCOMPARE(m_document->object(m_io)->value<double>(Prop::Value), 0.0);
        tap({50, 120}, touch); // Output mode
        QVERIFY(!m_input.isOpen());
        tap({50, 35}, touch);
        m_input.setText(u"101"_s);
        QVERIFY(!m_input.submit());
        m_input.setText(u"1.5"_s);
        QVERIFY(!m_input.submit());
        m_input.setText(u"nan"_s);
        QVERIFY(!m_input.submit());
        m_input.dismiss();
        QCOMPARE(m_engine.value(u"Setpoint"_s), TagValue(qint64(32)));
    }
    void sliderDrag_data() { devices(); }
    void sliderDrag()
    {
        QFETCH(bool, touch);
        const QPoint a = point({180, 30}), b = point({350, 30});
        if (touch)
        {
            QTest::touchEvent(m_window.get(), m_touch).press(0, a, m_window.get());
            QTest::touchEvent(m_window.get(), m_touch).move(0, b, m_window.get());
            QTest::touchEvent(m_window.get(), m_touch).release(0, b, m_window.get());
        }
        else
        {
            QTest::mousePress(m_window.get(), Qt::LeftButton, {}, a);
            QTest::mouseMove(m_window.get(), b);
            QTest::mouseRelease(m_window.get(), Qt::LeftButton, {}, b);
        }
        QTRY_VERIFY(TagValues::toNumber(m_engine.value(u"Setpoint"_s)).value_or(0) > 90);
        QTRY_VERIFY(play(m_gauge)->value<double>(Prop::Value) > 90);
    }
    void selectorsAndActions_data() { devices(); }
    void selectorsAndActions()
    {
        QFETCH(bool, touch);
        tap({50, 185}, touch);
        QCOMPARE(m_engine.value(u"Flag"_s), TagValue(true));
        tap({190, 185}, touch);
        QCOMPARE(m_engine.value(u"Flag"_s), TagValue(false));
        tap({60, 275}, touch);
        QCOMPARE(m_engine.value(u"Choice"_s), TagValue(qint64(1)));
        tap({230, 320}, touch);
        QCOMPARE(m_engine.value(u"Choice"_s), TagValue(qint64(2)));
        tap({430, 250}, touch);
        QVERIFY(m_input.isOpen());
        QCOMPARE(m_input.choices(), QStringList({u"A"_s, u"B"_s, u"C"_s}));
        QVERIFY(m_input.choose(0));
        QCOMPARE(m_engine.value(u"Choice"_s), TagValue(qint64(0)));
        tap({50, 430}, touch);
        QCOMPARE(m_engine.value(u"Setpoint"_s), TagValue(qint64(20)));
        tap({300, 430}, touch);
        QCOMPARE(m_engine.value(u"Setpoint"_s), TagValue(qint64(25)));
        tap({170, 430}, touch);
        QCOMPARE(m_engine.value(u"Flag"_s), TagValue(true));
        tap({560, 430}, touch);
        QCOMPARE(m_engine.value(u"Setpoint"_s), TagValue(qint64(80)));
    }
    void eventActionsRunInOrder_data() { devices(); }
    void eventActionsRunInOrder()
    {
        QFETCH(bool, touch);
        QSignalSpy requested(&m_input, &RuntimeInputHandler::screenRequested);
        tap({715, 430}, touch);
        QCOMPARE(m_engine.value(u"Setpoint"_s), TagValue(qint64(33)));
        QCOMPARE(requested.count(), 1);
        QCOMPARE(requested.front().front().toString(), u"Next"_s);

        const TagValue before = m_engine.value(u"Flag"_s);
        tap({60, 500}, touch); // the rectangle
        QVERIFY(m_engine.value(u"Flag"_s) != before);
    }
    void momentaryReleaseAndCancellation_data() { devices(); }
    void momentaryReleaseAndCancellation()
    {
        QFETCH(bool, touch);
        const auto p = point({440, 430});
        if (touch)
            QTest::touchEvent(m_window.get(), m_touch).press(0, p, m_window.get());
        else
            QTest::mousePress(m_window.get(), Qt::LeftButton, {}, p);
        QTRY_COMPARE(m_engine.value(u"Flag"_s), TagValue(true));
        if (touch)
            QTest::touchEvent(m_window.get(), m_touch)
                .release(0, point({700, 500}), m_window.get());
        else
            QTest::mouseRelease(m_window.get(), Qt::LeftButton, {}, point({700, 500}));
        QTRY_COMPARE(m_engine.value(u"Flag"_s), TagValue(false));
        if (touch)
            QTest::touchEvent(m_window.get(), m_touch).press(0, p, m_window.get());
        else
            QTest::mousePress(m_window.get(), Qt::LeftButton, {}, p);
        QTRY_COMPARE(m_engine.value(u"Flag"_s), TagValue(true));
        if (touch)
        {
            QTest::touchEvent(m_window.get(), m_touch)
                .stationary(0)
                .press(1, point({600, 450}), m_window.get());
        }
        else
            QTest::keyClick(m_window.get(), Qt::Key_Escape);
        QTRY_COMPARE(m_engine.value(u"Flag"_s), TagValue(false));
    }
    void inverseScaleAndDisplayOnlyTransform()
    {
        auto scale = TransformRegistry::instance().create(
            u"linear"_s, {{u"inMin"_s, 0}, {u"inMax"_s, 100}, {u"outMin"_s, 0}, {u"outMax"_s, 10}});
        QVERIFY(scale);
        QVERIFY(m_document->object(m_io)->setBinding({Prop::Value, u"Setpoint"_s, *scale}));
        m_view->setScreen(nullptr, nullptr);
        m_evaluator.show(*m_document);
        m_input.setScreen(m_evaluator.playScreen());
        m_view->setScreen(m_evaluator.playScreen(), &m_evaluator.playProject().notifier());
        tap({50, 35}, false);
        m_input.setText(u"4.2"_s);
        QVERIFY(m_input.submit());
        QCOMPARE(m_engine.value(u"Setpoint"_s), TagValue(qint64(42)));
        QTRY_COMPARE(play(m_io)->value<double>(Prop::Value), 4.2);
        auto expression =
            TransformRegistry::instance().create(u"expression"_s, {{u"text"_s, u"x * x"_s}});
        QVERIFY(expression);
        QVERIFY(!(*expression)->inverse(4.0));
    }
    void keyboardAndReadOnly()
    {
        tap({450, 70}, false); // a display has no operating focus
        QTest::keyClick(m_window.get(), Qt::Key_Tab);
        QCOMPARE(m_input.focusBounds(), play(m_io)->sceneBoundingRect());
        tap({200, 30}, false);
        const auto before = TagValues::toNumber(m_engine.value(u"Setpoint"_s)).value();
        QTest::keyClick(m_window.get(), Qt::Key_Right);
        QCOMPARE(TagValues::toNumber(m_engine.value(u"Setpoint"_s)).value(), before + 1);
        tap({440, 430}, false);
        QTest::keyPress(m_window.get(), Qt::Key_Space);
        QCOMPARE(m_engine.value(u"Flag"_s), TagValue(true));
        Q_EMIT m_view->inputCancelled();
        QCOMPARE(m_engine.value(u"Flag"_s), TagValue(false));
        QTest::keyRelease(m_window.get(), Qt::Key_Space);
        QCOMPARE(m_engine.value(u"Flag"_s), TagValue(false));
        class Monitor final : public IDriver
        {
        public:
            bool isReadOnly() const noexcept override { return true; }
            Result<void> start(std::span<const Tag>) override { return {}; }
            void stop() override {}
            Result<void> write(const QString&, const TagValue&) override { Q_UNREACHABLE(); }
            void poll(double, const Publish&) override {}
        };
        Tag tag;
        tag.name = u"Setpoint"_s;
        m_engine.start(std::vector<Tag>{tag}, std::make_unique<Monitor>());
        tap({50, 35}, false);
        QVERIFY(!m_input.isOpen());
        QVERIFY(m_input.error().contains(u"read only"));
        QVERIFY(!m_engine.writeChecked(u"Setpoint"_s, 20.0));
    }
    void groupedControlsShareTags()
    {
        m_input.setScreen(nullptr);
        m_view->setScreen(nullptr, nullptr);
        auto slider = m_document->takeObject(m_slider);
        auto gauge = m_document->takeObject(m_gauge);
        QVERIFY(slider && gauge);
        std::vector<std::unique_ptr<HmiObject>> children;
        children.push_back(std::move(*slider));
        children.push_back(std::move(*gauge));
        auto group = GroupObject::create(std::move(children));
        QVERIFY(group->setProperty(Prop::Rotation, 15.0));
        QVERIFY(group->setProperty(Prop::Width, group->geometry().width() * 0.8));
        const auto* child = group->children()[0].get();
        const auto transform =
            child->sceneTransform() * group->childTransform() * group->sceneTransform();
        const auto chosen = transform.map(QPointF(child->geometry().width() - 15, 20));
        const ObjectId root = group->id();
        QVERIFY(m_document->insertObject(std::move(group)));
        m_evaluator.show(*m_document);
        m_input.setScreen(m_evaluator.playScreen());
        m_view->setScreen(m_evaluator.playScreen(), &m_evaluator.playProject().notifier());
        QSignalSpy changed(&m_evaluator.playProject().notifier(),
                           &DocumentNotifier::propertyChanged);
        tap(chosen, false);
        QTRY_VERIFY(TagValues::toNumber(m_engine.value(u"Setpoint"_s)).value() > 90);
        const auto* playedGroup = play(root);
        QTRY_VERIFY(playedGroup->children()[1]->value<double>(Prop::Value) > 90);
        QVERIFY(!changed.isEmpty());
    }

    void checkedWritesRespectIntegerLimits()
    {
        Tag tag;
        tag.name = u"Counter"_s;
        tag.type = Tag::Type::Int;
        tag.maximum = 99.8;
        tag.limitInput = true;
        m_engine.start(std::vector<Tag>{tag});
        QVERIFY(!m_engine.writeChecked(tag.name, 99.7));
        QVERIFY(!m_engine.writeChecked(tag.name, u"1.5"_s));
        QVERIFY(!m_engine.writeChecked(tag.name, u"999999999999999999999999"_s));
        QVERIFY(m_engine.writeChecked(tag.name, qint64(42)));
        QCOMPARE(m_engine.value(tag.name), TagValue(qint64(42)));
        m_engine.stop();

        // Without limitInput, minimum and maximum are only the simulation
        // range: an operator may enter 150 into a 0..100 tag.
        Tag free;
        free.name = u"Free"_s;
        m_engine.start(std::vector<Tag>{free});
        QVERIFY(m_engine.writeChecked(free.name, 150.0));
        QCOMPARE(m_engine.value(free.name), TagValue(150.0));
    }
};
QTEST_MAIN(RuntimeInputTest)
#include "RuntimeInputTest.moc"
