#pragma once

#include <QWidget>
#include <QComboBox>
#include <QListWidget>
#include <QCheckBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include "aur/project.hpp"

namespace aur {

class TilesetEditorPanel : public QWidget {
    Q_OBJECT
    Project* project = nullptr;
    QComboBox* tsCombo = nullptr;
    QListWidget* tileList = nullptr;

public:
    explicit TilesetEditorPanel(Project* project = nullptr, QWidget* parent = nullptr);
    void setProject(Project* p);
    void refreshTilesets();
};

} // namespace aur
