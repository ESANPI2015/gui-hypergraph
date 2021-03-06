#include "HypergraphGUI.hpp"
#include "ui_HypergraphGUI.h"

#include "HypergraphViewer.hpp"
#include "ConceptgraphViewer.hpp"
#include "CommonConceptGraphViewer.hpp"
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

    mpViewerTabWidget = new QTabWidget(this);
    setCentralWidget(mpViewerTabWidget);

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
    connect(mpControl, SIGNAL(newHypergraph(HypergraphType)), this, SLOT(newHypergraphRequest(HypergraphType)));
    connect(mpControl, SIGNAL(loadHypergraph(HypergraphType)), this, SLOT(loadHypergraphRequest(HypergraphType)));
    connect(mpControl, SIGNAL(storeHypergraph()), this, SLOT(storeHypergraphRequest()));
    connect(mpControl, SIGNAL(setEquilibriumDistance(qreal)), this, SLOT(setEquilibriumDistanceRequest(qreal)));
}

HypergraphGUI::~HypergraphGUI()
{
    delete mpUi;
}

void HypergraphGUI::setEquilibriumDistanceRequest(qreal distance)
{
    HypergraphViewer* mpHypergraphViewer = dynamic_cast<HypergraphViewer*>(mpViewerTabWidget->currentWidget());
    if (mpHypergraphViewer)
    {
        mpHypergraphViewer->setEquilibriumDistance(distance);
    }
}

void HypergraphGUI::clearHypergraphRequest()
{
    // If there is a tab widget, destroy it
    if (mpViewerTabWidget && (mpViewerTabWidget->currentIndex() > -1))
    {
        mpViewerTabWidget->removeTab(mpViewerTabWidget->currentIndex());
    }
}

void HypergraphGUI::newHypergraphRequest(HypergraphType type)
{
    switch (type)
    {
        case COMMONCONCEPTGRAPH:
            {
                CommonConceptGraphWidget* commonConceptGraphWidget = new CommonConceptGraphWidget();
                mpViewerTabWidget->addTab(commonConceptGraphWidget, "CommonConceptGraph");
                connect(commonConceptGraphWidget, SIGNAL(YAMLStringReady(const QString&)), this, SLOT(onYAMLStringReady(const QString&)));
                CommonConceptGraph empty;
                commonConceptGraphWidget->loadFromGraph(empty);
            }
            break;
        case CONCEPTGRAPH:
            {
                ConceptgraphWidget* conceptGraphWidget = new ConceptgraphWidget();
                mpViewerTabWidget->addTab(conceptGraphWidget, "Conceptgraph");
                connect(conceptGraphWidget, SIGNAL(YAMLStringReady(const QString&)), this, SLOT(onYAMLStringReady(const QString&)));
                Conceptgraph empty;
                conceptGraphWidget->loadFromGraph(empty);
            }
            break;
        default:
            {
                HypergraphViewer* hypergraphViewer = new HypergraphViewer();
                mpViewerTabWidget->addTab(hypergraphViewer, "Hypergraph");
                connect(hypergraphViewer, SIGNAL(YAMLStringReady(const QString&)), this, SLOT(onYAMLStringReady(const QString&)));
                Conceptgraph empty;
                hypergraphViewer->loadFromGraph(empty);
            }
            break;
    }
}

void HypergraphGUI::loadHypergraphRequest(HypergraphType type)
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

    // ... if everything is ok, create a viewer
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
            switch (type)
            {
                case COMMONCONCEPTGRAPH:
                    {
                        CommonConceptGraphWidget* commonConceptGraphWidget = new CommonConceptGraphWidget();
                        mpViewerTabWidget->addTab(commonConceptGraphWidget, "CommonConceptGraph");
                        connect(commonConceptGraphWidget, SIGNAL(YAMLStringReady(const QString&)), this, SLOT(onYAMLStringReady(const QString&)));
                        commonConceptGraphWidget->loadFromYAML(yamlString);
                    }
                    break;
                case CONCEPTGRAPH:
                    {
                        ConceptgraphWidget* conceptGraphWidget = new ConceptgraphWidget();
                        mpViewerTabWidget->addTab(conceptGraphWidget, "Conceptgraph");
                        connect(conceptGraphWidget, SIGNAL(YAMLStringReady(const QString&)), this, SLOT(onYAMLStringReady(const QString&)));
                        conceptGraphWidget->loadFromYAML(yamlString);
                    }
                    break;
                default:
                    {
                        HypergraphViewer* hypergraphViewer = new HypergraphViewer();
                        mpViewerTabWidget->addTab(hypergraphViewer, "Hypergraph");
                        connect(hypergraphViewer, SIGNAL(YAMLStringReady(const QString&)), this, SLOT(onYAMLStringReady(const QString&)));
                        hypergraphViewer->loadFromYAML(yamlString);
                    }
                    break;
            }
        } else {
            // Opening failed
        }
    }
}

void HypergraphGUI::storeHypergraphRequest()
{
    if (mpViewerTabWidget->currentIndex() < 0)
        return;

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
        // Call the currently visible viewer
        HypergraphViewer* mpHypergraphViewer = dynamic_cast<HypergraphViewer*>(mpViewerTabWidget->currentWidget());
        mpHypergraphViewer->storeToYAML();
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
