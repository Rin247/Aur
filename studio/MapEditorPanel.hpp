#pragma once

#include <QWidget>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QPainter>
#include <QMouseEvent>
#include <QEvent>
#include <QRectF>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QPointF>
#include <map>
#include <vector>
#include "aur/project.hpp"
#include "aur/render.hpp"

namespace aur {

class MapCanvasWidget : public QWidget {
    Q_OBJECT
    Project* project = nullptr;
    std::string current_map;
    int current_layer = 0; // 0=ground, 1=objects, 2=above, 3=collision overlay
    int selected_tile = 0;
    int hover_x = -1, hover_y = -1;
    bool selecting_entity = false;
    int selected_entity_index = -1;
    std::map<std::pair<std::string, int>, QImage> tile_cache;
    qreal zoom_factor = 1.0;
    QPointF pan_offset;
    QPointF pan_start_position;
    QPointF pan_start_offset;
    bool panning = false;
    int active_tool = 0;
    void constrainCamera();

public:
    QRectF mapBounds() const;

private:
    std::vector<std::pair<int, Json>> undo_layers;
    std::vector<std::pair<int, Json>> redo_layers;
    bool stroke_active = false;

    QPoint tileAt(const QPointF& position) const;
    QImage tileImage(const std::string& tileset_id, int tile_index);
    void paintTile(const QPoint& tile);
    Json* editableLayer(int layer_index);

public:
    explicit MapCanvasWidget(QWidget* parent = nullptr);
    void setProject(Project* p);
    void setMap(const std::string& mid);
    void setLayer(int l);
    void setTool(int tool_index);
    void setSelectedTile(int t);
    void setSelectedEntityKind(int entity_kind);
    qreal zoomFactor() const noexcept { return zoom_factor; }
    QPointF panOffset() const noexcept { return pan_offset; }
    void fitToView();
    void centerOn(const QPointF& position);
    void zoomIn();
    void zoomOut();
    void undo();
    void redo();

signals:
    void zoomChanged(qreal zoom);
    void entitySelectionRequested(int entity_index);


protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void leaveEvent(QEvent* event) override;
};

class MapEditorPanel : public QWidget {
    Q_OBJECT
    Project* project = nullptr;
    QComboBox* mapCombo = nullptr;
    QComboBox* modeCombo = nullptr;   // Real-Time Action / Turn-Based / Tactical
    QComboBox* layerCombo = nullptr;
    QComboBox* toolCombo = nullptr;
    QListWidget* entityTypeWidget = nullptr;
    MapCanvasWidget* canvas = nullptr;
    QPushButton* fitButton = nullptr;
    QPushButton* zoomInButton = nullptr;
    QPushButton* zoomOutButton = nullptr;
    QLabel* zoomLabel = nullptr;
    QLabel* modeHint = nullptr;
    QLabel* selectedObjectLabel = nullptr;
    QLineEdit* facingEdit = nullptr;
    QComboBox* eventCombo = nullptr;
    QPushButton* eventApplyButton = nullptr;
    std::string current_map;


public:
    explicit MapEditorPanel(Project* project = nullptr, QWidget* parent = nullptr);
    void setProject(Project* p);
    void refreshMaps();
};

} // namespace aur
