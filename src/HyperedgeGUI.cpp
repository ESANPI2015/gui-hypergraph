#include "HyperedgeGUI.hpp"
#include "ui_HyperedgeGUI.h"

#include "HyperedgeViewer.hpp"
#include <QDockWidget>

HyperedgeGUI::HyperedgeGUI(QWidget *parent)
    : QMainWindow(parent)
{
    mpUi = new Ui::HyperedgeGUI();
    mpUi->setupUi(this);

    mpViewer = new HyperedgeViewer();
    setCentralWidget(mpViewer);

    QDockWidget *dockWidget = new QDockWidget(tr("Dock Widget"), this);
    dockWidget->setAllowedAreas(Qt::LeftDockWidgetArea |
                                Qt::RightDockWidgetArea);
    //dockWidget->setWidget(dockWidgetContents);
    addDockWidget(Qt::LeftDockWidgetArea, dockWidget);
}

HyperedgeGUI::~HyperedgeGUI()
{
    delete mpUi;
}
