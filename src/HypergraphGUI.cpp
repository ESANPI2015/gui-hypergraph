#include "HypergraphGUI.hpp"
#include "ui_HypergraphGUI.h"

#include "HypergraphViewer.hpp"
#include "ConceptgraphViewer.hpp"
#include "HypergraphControl.hpp"
#include <QDockWidget>
#include <QTabWidget>
#include <QFileDialog>
#include <QTextStream>
#include "Hyperedge.hpp"

HypergraphGUI::HypergraphGUI(QWidget *parent)
    : QMainWindow(parent)
{
    mpUi = new Ui::HypergraphGUI();
    mpUi->setupUi(this);

    QTabWidget *allViewers = new QTabWidget(this);
    setCentralWidget(allViewers);

    mpHedgeViewer = new HypergraphViewer();
    allViewers->addTab(mpHedgeViewer, "Hypergraph");
    mpConceptViewer = new ConceptgraphViewer();
    allViewers->addTab(mpConceptViewer, "Conceptgraph");

    QDockWidget *dockWidget = new QDockWidget(NULL, this);
    dockWidget->setAllowedAreas(Qt::LeftDockWidgetArea |
                                Qt::RightDockWidgetArea);
    mpControl = new HypergraphControl();
    dockWidget->setWidget(mpControl);
    addDockWidget(Qt::LeftDockWidgetArea, dockWidget);

    lastOpenedFile = "";
    lastSavedFile = "";

    // Connect control
    connect(mpControl, SIGNAL(clearHypergraph()), this, SLOT(clearHypergraphRequest()));
    connect(mpControl, SIGNAL(loadHypergraph()), this, SLOT(loadHypergraphRequest()));
    connect(mpControl, SIGNAL(storeHypergraph()), this, SLOT(storeHypergraphRequest()));

    // Connect viewer
    connect(mpConceptViewer, SIGNAL(YAMLStringReady(const QString&)), this, SLOT(onYAMLStringReady(const QString&)));
    connect(mpHedgeViewer, SIGNAL(YAMLStringReady(const QString&)), this, SLOT(onYAMLStringReady(const QString&)));

    // TODO: The viewers have to be synchronized. If one changes the underlying graph the other has to reload that one.
}

HypergraphGUI::~HypergraphGUI()
{
    delete mpUi;
}

void HypergraphGUI::clearHypergraphRequest()
{
    mpConceptViewer->clearConceptgraph();
    mpHedgeViewer->clearHypergraph();
}

void HypergraphGUI::loadHypergraphRequest()
{
    // Give standard dir or last used dir
    QString lastDir = QDir::currentPath();
    if (!lastOpenedFile.isEmpty())
    {
        lastDir = QFileInfo(lastOpenedFile).absolutePath();
    }

    // Open a dialog
    auto fileName = QFileDialog::getOpenFileName(this,
        tr("Open Hypergraph YAML"), lastDir, tr("YAML Files (*.yml *.yaml)"));

    // ... if everything is ok, pass request to mpConceptViewer
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
            mpConceptViewer->loadFromYAML(yamlString);
            mpHedgeViewer->loadFromYAML(yamlString);
        } else {
            // Opening failed
        }    
    }
}

void HypergraphGUI::storeHypergraphRequest()
{
    // Handle lastSavedFile
    if (lastSavedFile.isEmpty())
        lastSavedFile = lastOpenedFile;

    // Open a dialog
    auto fileName = QFileDialog::getSaveFileName(this, tr("Save Hypergraph YAML"),
                               lastSavedFile,
                               tr("YAML Files (*.yml *.yaml)"));

    // ... if everything is ok, pass request to the currently active viewer
    if (fileName != "")
    {   
        lastSavedFile = fileName;
        // TODO: Store dependent on the currently active tab!!!
        mpConceptViewer->storeToYAML();
    }
}
void HypergraphGUI::onYAMLStringReady(const QString& yamlString)
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
