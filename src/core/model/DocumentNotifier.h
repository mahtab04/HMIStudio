#pragma once

#include "core/model/Ids.h"

#include <QObject>

namespace hmi
{

// The single QObject of a document. Model classes are plain C++ so they stay
// light and cloneable; every change to them is announced here instead, and
// the Qt Quick items and panel models listen to these signals.
class DocumentNotifier : public QObject
{
    Q_OBJECT

public:
    explicit DocumentNotifier(QObject* parent = nullptr);

Q_SIGNALS:
    void screenAdded(hmi::ScreenId screen, int index);
    void screenRemoved(hmi::ScreenId screen);
    void screenChanged(hmi::ScreenId screen);

    void objectAdded(hmi::ScreenId screen, hmi::ObjectId object, int index);
    void objectRemoved(hmi::ScreenId screen, hmi::ObjectId object);
    void objectsReordered(hmi::ScreenId screen);

    void propertyChanged(hmi::ObjectId object, hmi::PropertyId property);
};

} // namespace hmi

Q_DECLARE_METATYPE(hmi::ScreenId)
Q_DECLARE_METATYPE(hmi::ObjectId)
Q_DECLARE_METATYPE(hmi::PropertyId)
