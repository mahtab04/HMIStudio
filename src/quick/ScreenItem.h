#pragma once

#include "core/model/Ids.h"

#include <QHash>
#include <QQuickItem>

namespace hmi
{

class DocumentNotifier;
class IGraphicProvider;
class ObjectItem;
class Screen;

// Qt Quick counterpart of one Screen: keeps one ObjectItem per object, in
// z-order, in step with the document through the DocumentNotifier.
// Positioned and scaled by CanvasItem; scene coordinates are its local
// coordinates. GridItem draws the background below it, so the grid can sit
// between the background and the objects.
class ScreenItem : public QQuickItem
{
    Q_OBJECT

public:
    explicit ScreenItem(QQuickItem* parent = nullptr);
    ~ScreenItem() override;
    ScreenItem(const ScreenItem&) = delete;
    ScreenItem& operator=(const ScreenItem&) = delete;
    ScreenItem(ScreenItem&&) = delete;
    ScreenItem& operator=(ScreenItem&&) = delete;

    // Shows `screen` (nullptr clears). The notifier must outlive the binding.
    void setScreen(const Screen* screen, DocumentNotifier* notifier);
    [[nodiscard]] const Screen* screen() const noexcept { return m_screen; }

    [[nodiscard]] ObjectItem* objectItem(ObjectId id) const { return m_items.value(id); }
    [[nodiscard]] int objectItemCount() const { return int(m_items.size()); }

    // Texture scale for objects that intersect `sharpArea` (scene
    // coordinates); objects further away render at no more than 1:1, so a
    // deep zoom does not hold full-resolution textures for the whole screen.
    void setRenderScale(double scale, const QRectF& sharpArea);
    void setGraphics(const IGraphicProvider* graphics);

    void showPreview(ObjectId id, const QRectF& sceneGeometry, double rotation);
    void clearPreview();

Q_SIGNALS:
    // The screen's objects, z-order or geometry changed; overlays refresh.
    void contentChanged();

private:
    void rebuild();
    void addItem(ObjectId id);
    void removeItem(ObjectId id);
    void syncZOrder();
    void syncScreen();
    void onPropertyChanged(ObjectId id);
    [[nodiscard]] double renderScaleFor(const ObjectItem& item) const;

    const Screen* m_screen = nullptr;
    DocumentNotifier* m_notifier = nullptr;
    QHash<ObjectId, ObjectItem*> m_items;
    QList<ObjectId> m_previewed;
    double m_renderScale = 1.0;
    QRectF m_sharpArea;
    const IGraphicProvider* m_graphics = nullptr;
};

} // namespace hmi
