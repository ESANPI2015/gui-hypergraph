#include "HyperedgeGUI.hpp"
#include "ui_HyperedgeGUI.h"

#include "HyperedgeViewer.hpp"
#include "HyperedgeControl.hpp"
#include <QDockWidget>
#include <QFileDialog>
#include <QTextStream>
#include "Hyperedge.hpp"

HyperedgeGUI::HyperedgeGUI(QWidget *parent)
    : QMainWindow(parent)
{
    mpUi = new Ui::HyperedgeGUI();
    mpUi->setupUi(this);

    mpViewer = new HyperedgeViewer();
    setCentralWidget(mpViewer);

    QDockWidget *dockWidget = new QDockWidget(NULL, this);
    dockWidget->setAllowedAreas(Qt::LeftDockWidgetArea |
                                Qt::RightDockWidgetArea);
    mpControl = new HyperedgeControl();
    dockWidget->setWidget(mpControl);
    addDockWidget(Qt::LeftDockWidgetArea, dockWidget);

    lastOpenedFile = "";
    lastSavedFile = "";

    // Connect control
    connect(mpControl, SIGNAL(clearHyperedgeSystem()), this, SLOT(clearHyperedgeSystemRequest()));
    connect(mpControl, SIGNAL(loadHyperedgeSystem()), this, SLOT(loadHyperedgeSystemRequest()));
    connect(mpControl, SIGNAL(storeHyperedgeSystem()), this, SLOT(storeHyperedgeSystemRequest()));

    // Connect viewer
    connect(mpViewer, SIGNAL(YAMLStringReady(const QString&)), this, SLOT(onYAMLStringReady(const QString&)));
}

HyperedgeGUI::~HyperedgeGUI()
{
    delete mpUi;
}

void HyperedgeGUI::clearHyperedgeSystemRequest()
{
    mpViewer->clearHyperedgeSystem();
}

void HyperedgeGUI::loadHyperedgeSystemRequest()
{
    // Give standard dir or last used dir
    QString lastDir = QDir::currentPath();
    if (!lastOpenedFile.isEmpty())
    {
        lastDir = QFileInfo(lastOpenedFile).absolutePath();
    }

    // Open a dialog
    auto fileName = QFileDialog::getOpenFileName(this,
        tr("Open Hyperedge YAML"), lastDir, tr("YAML Files (*.yml *.yaml)"));

    // ... if everything is ok, pass request to mpViewer
    if (fileName != "")
    {   
        // Qt way
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            // Successfully opened
            QTextStream fin(&file);
            QString yamlString = fin.readAll();
            file.close();
            lastOpenedFile = fileName;
            mpViewer->loadFromYAML(yamlString);
        } else {
            // Opening failed
        }    
    }
}

void HyperedgeGUI::storeHyperedgeSystemRequest()
{
    // Handle lastSavedFile
    if (lastSavedFile.isEmpty())
        lastSavedFile = lastOpenedFile;

    // Open a dialog
    auto fileName = QFileDialog::getSaveFileName(this, tr("Save Hyperedge YAML"),
                               lastSavedFile,
                               tr("YAML Files (*.yml *.yaml)"));

    // ... if everything is ok, pass request to mpViewer
    if (fileName != "")
    {   
        lastSavedFile = fileName;
        mpViewer->storeToYAML();
    }
}
void HyperedgeGUI::onYAMLStringReady(const QString& yamlString)
{
    if (!lastSavedFile.isEmpty())
    {
        // Qt way
        QFile file(lastSavedFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            // Successfully opened
            QTextStream fout(&file);
            fout << yamlString;
            file.close();
        } else {
            // Opening failed
        }
    }
}
