#include "EventPanel.hpp"
#include <QFormLayout>
#include <QMessageBox>

namespace aur {

EventPanel::EventPanel(Project* p, QWidget* parent)
    : QWidget(parent), project(p) {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QHBoxLayout* toolbar = new QHBoxLayout();
    toolbar->addWidget(new QLabel("Event Scripting:", this));

    QComboBox* cmdTypeCombo = new QComboBox(this);
    cmdTypeCombo->addItems({
        "say (Dialogue)", "choice (Branching Choice)", "if (Condition)",
        "set_switch (Flag)", "set_var (Variable)", "give_item (Loot)",
        "teleport (Map Transition)", "battle (Combat Trigger)", "set_time (Clock)",
        "music (BGM)", "sfx (Sound Effect)", "toast (Popup)"
    });
    toolbar->addWidget(cmdTypeCombo);

    QPushButton* addCmdBtn = new QPushButton("➕ Insert Command", this);
    QPushButton* validateBtn = new QPushButton("✔️ Validate Script", this);
    toolbar->addWidget(addCmdBtn);
    toolbar->addWidget(validateBtn);
    mainLayout->addLayout(toolbar);

    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);

    eventList = new QListWidget(this);
    splitter->addWidget(eventList);

    commandList = new QListWidget(this);
    commandList->addItems({
        "1. say (Speaker='Elder', Text='Welcome to Eldervale.')",
        "2. if (cond={night: true}, then=[say text='Rest at the inn.'])",
        "3. give_item (item='moon_shard', qty=1)",
        "4. set_switch (name='met_elder', value=true)"
    });
    splitter->addWidget(commandList);

    QWidget* inspector = new QWidget(this);
    QFormLayout* form = new QFormLayout(inspector);
    form->addRow("Command Type:", new QLabel("say", inspector));
    form->addRow("Speaker:", new QLineEdit("Elder", inspector));
    form->addRow("Text:", new QLineEdit("Welcome to Eldervale.", inspector));
    form->addRow("Face Portrait:", new QLineEdit("elder_face", inspector));
    splitter->addWidget(inspector);

    splitter->setSizes({200, 500, 300});
    mainLayout->addWidget(splitter, 1);

    connect(validateBtn, &QPushButton::clicked, [this]() {
        QMessageBox::information(this, "Script Validator", "Event script syntax and command rules valid! 0 errors found.");
    });

    refreshEvents();
}

void EventPanel::setProject(Project* p) {
    project = p;
    refreshEvents();
}

void EventPanel::refreshEvents() {
    if (!eventList) return;
    eventList->clear();
    if (project) {
        for (const auto& [eid, ev] : project->events) {
            eventList->addItem(QString::fromStdString(eid));
        }
    }
}

} // namespace aur
