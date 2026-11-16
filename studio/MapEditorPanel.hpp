#pragma once

// MapEditorPanel.hpp - the map editor's panel: the toolbars, the docks and
// the inspector around the canvas.

#include <QWidget>
#include <QComboBox>
#include <QListWidget>

#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>

#include <QFormLayout>
#include <functional>

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
#include <QUndoStack>
#include <memory>

#include "map/MapCanvas.hpp"

#include <map>

#include <vector>
#include "aur/project.hpp"
#include "aur/render.hpp"
#include "map/TilePaletteWidget.hpp"

namespace aur {

class MapEditorPanel : public QWidget {
    Q_OBJECT
    Project* project = nullptr;
    QComboBox* mapCombo = nullptr;
    QComboBox* modeCombo = nullptr;   // Real-Time Action / Turn-Based / Tactical
    QComboBox* layerCombo = nullptr;
    QComboBox* toolCombo = nullptr;
    QListWidget* entityTypeWidget = nullptr;
    TilePaletteWidget* tilePalette = nullptr;
    MapCanvasWidget* canvas = nullptr;
    QPushButton* fitButton = nullptr;
    QPushButton* zoomInButton = nullptr;
    QPushButton* zoomOutButton = nullptr;
    QLabel* zoomLabel = nullptr;
    QLabel* selectedObjectLabel = nullptr;
    QLineEdit* facingEdit = nullptr;
    QComboBox* eventCombo = nullptr;
    QPushButton* eventApplyButton = nullptr;
    QLineEdit* idEdit = nullptr;        // D3.S1: selected entity's id
    QLineEdit* nameEdit = nullptr;      // D3.S1: display name / text field
    QLineEdit* textEdit = nullptr;      // D3.S1: sign/dialogue text
    QSpinBox* paramSpin = nullptr;      // D3.S1: generic integer payload
    QPushButton* entityDeleteButton = nullptr;
    std::function<void(int)> syncInspectorFromInstance;
    std::string current_map;


public:
    explicit MapEditorPanel(Project* project = nullptr, QWidget* parent = nullptr);
    void setProject(Project* p);
    void refreshMaps();
    // Opens the map with this id, if the project has it.
    void openMap(const QString& id);
    void refreshTilePalette();
    void onTilesetChanged();  // flag edits elsewhere invalidate the tile cache
    // The zoom control, as a factor of the fit.
    void setEditorZoom(qreal factor);
};

} // namespace aur
