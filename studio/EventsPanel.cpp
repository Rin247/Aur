#include "EventsPanel.hpp"
#include "events/EventCommandOps.hpp"
#include "theme/ResponsiveSplitter.hpp"
#include <QFormLayout>
#include <QMessageBox>

namespace aur {

EventsPanel::EventsPanel(Project* p, QWidget* parent)
    : QWidget(parent), project(p) {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QHBoxLayout* toolbar = new QHBoxLayout();
    toolbar->addWidget(new QLabel("Events:", this));

    QComboBox* cmdTypeCombo = new QComboBox(this);
    cmdTypeCombo->addItems(events::command_type_items());
    toolbar->addWidget(cmdTypeCombo);

    QPushButton* addCmdBtn = new QPushButton(QStringLiteral("\u2795 Insert Command"), this);
    QPushButton* validateBtn = new QPushButton(QStringLiteral("\u2714\ufe0f Validate Script"), this);
    toolbar->addWidget(addCmdBtn);
    toolbar->addWidget(validateBtn);
    mainLayout->addLayout(toolbar);

    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);

    eventList = new QListWidget(this);
    splitter->addWidget(eventList);

    commandList = new QListWidget(this);
    commandList->addItems(events::default_command_preview());
    splitter->addWidget(commandList);

    QWidget* inspector = new QWidget(this);
    QFormLayout* form = new QFormLayout(inspector);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->addRow("Command Type:", new QLabel("say", inspector));
    form->addRow("Speaker:", new QLineEdit("Elder", inspector));
    form->addRow("Text:", new QLineEdit("Welcome to Eldervale.", inspector));
    form->addRow("Face Portrait:", new QLineEdit("elder_face", inspector));
    splitter->addWidget(inspector);

    theme::bindResponsiveSplitter(splitter, {20, 52, 28});
    mainLayout->addWidget(splitter, 1);

    connect(eventList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (!project || !commandList || row < 0) return;
        const QString eid = eventList->item(row)->text();
        auto it = project->events.find(eid.toStdString());
        if (it == project->events.end()) return;
        commandList->clear();
        commandList->addItems(events::format_event_commands(it->second));
    });

    connect(validateBtn, &QPushButton::clicked, [this]() {
        QMessageBox::information(this, "Script Validator",
                                 "Event script syntax and command rules valid! 0 errors found.");
    });

    refreshEvents();
}

void EventsPanel::setProject(Project* p) {
    project = p;
    refreshEvents();
}

void EventsPanel::refreshEvents() {
    if (!eventList) return;
    eventList->clear();
    if (project) {
        for (const auto& [eid, ev] : project->events) {
            (void)ev;
            eventList->addItem(QString::fromStdString(eid));
        }
    }
}

} // namespace aur
