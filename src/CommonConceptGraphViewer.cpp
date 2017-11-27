#include "CommonConceptGraphViewer.hpp"
#include "ui_CommonConceptGraphViewer.h"
#include "ui_HypergraphViewer.h"
#include "CommonConceptGraphItem.hpp"

#include <QGraphicsScene>
#include <QWheelEvent>
#include <QTimer>
#include <QtCore>

#include "Hypergraph.hpp"
#include "Conceptgraph.hpp"
#include "CommonConceptGraph.hpp"
#include "HyperedgeYAML.hpp"
#include <sstream>
#include <iostream>

CommonConceptGraphScene::CommonConceptGraphScene(QObject * parent)
: ConceptgraphScene(parent)
{
    mShowClasses = false;
    mShowInstances = true;
}

CommonConceptGraphScene::~CommonConceptGraphScene()
{
}

QList<CommonConceptGraphItem*> CommonConceptGraphScene::selectedCommonConceptGraphItems()
{
    // get all selected hyperedge items
    QList<QGraphicsItem *> selItems = selectedItems();
    QList<CommonConceptGraphItem *> selHItems;
    for (QGraphicsItem *item : selItems)
    {
        CommonConceptGraphItem* hitem = dynamic_cast<CommonConceptGraphItem*>(item);
        if (hitem)
            selHItems.append(hitem);
    }
    return selHItems;
}

void CommonConceptGraphScene::addItem(QGraphicsItem *item)
{
    ConceptgraphScene::addItem(item);

    // Emit signals
    CommonConceptGraphItem *edge = dynamic_cast<CommonConceptGraphItem*>(item);
    if (edge)
    {
        emit conceptAdded(edge->getHyperEdgeId());
    }
}

void CommonConceptGraphScene::removeItem(QGraphicsItem *item)
{
    // Emit signals
    CommonConceptGraphItem *edge = dynamic_cast<CommonConceptGraphItem*>(item);
    if (edge)
    {
        emit conceptRemoved(edge->getHyperEdgeId());
    }

    ConceptgraphScene::removeItem(item);
}

void CommonConceptGraphScene::addConcept(const UniqueId id, const QString& label)
{
    //CommonConceptGraph *g = graph();
    //if (g)
    //{
    //    UniqueId theId = id > 0 ? id : qHash(label);
    //    while (g->create(theId, label.toStdString()).empty()) theId++;
    //    visualize();
    //}
}

void CommonConceptGraphScene::addRelation(const UniqueId fromId, const UniqueId toId, const UniqueId id, const QString& label)
{
    //CommonConceptGraph *g = graph();
    //if (g)
    //{
    //    UniqueId theId = id > 0 ? id : qHash(label);
    //    while (g->relate(theId, fromId, toId, label.toStdString()).empty()) theId++;
    //    visualize();
    //}
}

void CommonConceptGraphScene::removeEdge(const UniqueId id)
{
    CommonConceptGraph *g = graph();
    if (g)
    {
        g->destroy(id);
        visualize();
    }
}

void CommonConceptGraphScene::updateEdge(const UniqueId id, const QString& label)
{
    CommonConceptGraph *g = graph();
    if (g)
    {
        g->get(id)->updateLabel(label.toStdString());
        visualize();
    }
}

void CommonConceptGraphScene::showClasses(const bool value)
{
    mShowClasses = value;
    visualize();
}

void CommonConceptGraphScene::showInstances(const bool value)
{
    mShowInstances = value;
    visualize();
}

void CommonConceptGraphScene::visualize(CommonConceptGraph* graph)
{
    // If a new graph is given, ...
    if (graph)
    {
        if (currentGraph)
        {
            // Merge graphs
            Hypergraph merged = Hypergraph(*currentGraph, *graph);
            Conceptgraph mergedCG(merged);
            CommonConceptGraph* mergedGraph = new CommonConceptGraph(mergedCG);
            // destroy old one
            delete currentGraph;
            currentGraph = mergedGraph;
        } else {
            currentGraph = new CommonConceptGraph(*graph);
        }
    }

    // If we dont have any graph ... skip
    if (!this->graph())
        return;

    // Suppress visualisation if desired
    if (!isEnabled())
        return;

    // Select either instances or classes
    auto allConcepts = this->graph()->find();
    auto allInstances = this->graph()->instancesOf(allConcepts);
    auto allClasses   = subtract(allConcepts, allInstances);

    if (!mShowClasses)
    {
        allConcepts = subtract(allConcepts, allClasses);
        allClasses.clear();
    }
    if (!mShowInstances)
    {
        allConcepts = subtract(allConcepts, allInstances);
        allInstances.clear();
    }

    // First pass: Sort instances into different sets
    QMap< UniqueId, Hyperedges > childrenOfParent;
    QMap< UniqueId, Hyperedges > partsOfWhole;
    QMap< UniqueId, Hyperedges > superclassesOf;
    QMap< UniqueId, Hyperedges > endpointsOfConnector;
    // For all
    for (auto conceptId : allConcepts)
    {
        // Find children
        Hyperedges children = this->graph()->childrenOf(Hyperedges{conceptId});
        children.erase(conceptId);
        childrenOfParent[conceptId] = children;
        // Find parts
        Hyperedges parts = this->graph()->partsOf(Hyperedges{conceptId});
        parts.erase(conceptId);
        partsOfWhole[conceptId] = parts;
        // Find endpoints
        Hyperedges endpoints = this->graph()->endpointsOf(Hyperedges{conceptId});
        endpoints.erase(conceptId);
        endpointsOfConnector[conceptId] = endpoints;
    }
    // For classes
    for (auto classId : allClasses)
    {
        // Find superclasses
        Hyperedges superclasses = this->graph()->subclassesOf(Hyperedges{classId},"",CommonConceptGraph::TraversalDirection::DOWN);
        superclasses.erase(classId);
        superclassesOf[classId] = superclasses;
    }
    // For instances
    for (auto instanceId : allInstances)
    {
        // Find superclasses
        Hyperedges superclasses = this->graph()->instancesOf(Hyperedges{instanceId},"",CommonConceptGraph::TraversalDirection::DOWN);
        superclasses.erase(instanceId);
        superclassesOf[instanceId] = superclasses;
    }

    // Remove parts from allConcepts
    QMap< UniqueId, Hyperedges >::const_iterator it;
    for (it = partsOfWhole.begin(); it != partsOfWhole.end(); ++it)
    {
        allConcepts = subtract(allConcepts, it.value());
    }

    int N = allConcepts.size();
    int dim = N * mEquilibriumDistance / 2;
    //std::cout << "#Instances\\Parts: " << N << "\n";

    // Second pass: Draw all instances except the parts
    QMap<UniqueId,CommonConceptGraphItem*> validItems;
    for (auto conceptId : allConcepts)
    {
        // Create or get item
        CommonConceptGraphItem *item;
        // Create superclass label
        std::string superclassLabel;
        for (auto superclassId : superclassesOf[conceptId])
        {
            superclassLabel += (" " + this->graph()->get(superclassId)->label());
        }
        if (!currentItems.contains(conceptId))
        {
            // Check if the concept is a class or an instance
            if (allInstances.count(conceptId))
            {
                item = new CommonConceptGraphItem(this->graph()->get(conceptId), CommonConceptGraphItem::INSTANCE, superclassLabel);
            } else {
                item = new CommonConceptGraphItem(this->graph()->get(conceptId), CommonConceptGraphItem::CLASS, superclassLabel);
            }
            item->setPos(qrand() % dim - dim/2, qrand() % dim - dim/2);
            addItem(item);
            currentItems[conceptId] = item;
        } else {
            item = dynamic_cast<CommonConceptGraphItem*>(currentItems[conceptId]);
        }
        // Make sure that the labels are up-to-date
        item->setLabel(QString::fromStdString(this->graph()->get(conceptId)->label()), QString::fromStdString(superclassLabel));
        validItems[conceptId] = item;
    }

    // Third pass:
    for (auto conceptId : allConcepts)
    {
        CommonConceptGraphItem *srcItem = dynamic_cast<CommonConceptGraphItem*>(validItems[conceptId]);
        if (!srcItem)
            continue;
        // Every child shall be attached to its parent
        for (auto childId : childrenOfParent[conceptId])
        {
            // A child is in validItems, currentItems and visible ... But we want it to be part of its parent
            CommonConceptGraphItem *destItem = dynamic_cast<CommonConceptGraphItem*>(validItems[childId]);
            if (!destItem)
                continue;
            // Omit loops
            if (srcItem == destItem)
                continue;
            // Check if this item is already part of the parent
            if (!srcItem->isAncestorOf(destItem))
            {
                destItem->setParentItem(srcItem);
            }
        }
        // Every connector shall be wired to its endpoints
        for (auto endpointId : endpointsOfConnector[conceptId])
        {
            // a connector points to each one of its connectors
            CommonConceptGraphItem *destItem = dynamic_cast<CommonConceptGraphItem*>(validItems[endpointId]);
            if (!destItem)
                continue;
            // Omit loops
            if (srcItem == destItem)
                continue;
            // Check if there is an edgeitem of type TO which points to destItem
            bool found = false;
            auto myEdgeItems = srcItem->getEdgeItems();
            for (auto line : myEdgeItems)
            {
                if (line->getTargetItem() == destItem)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                auto line = new CommonConceptGraphEdgeItem(srcItem, destItem, CommonConceptGraphEdgeItem::TO, CommonConceptGraphEdgeItem::SOLID_CURVED);
                addItem(line);
            }
        }
        // Every instance/class shall be related to its superclasses
        for (auto superclassId : superclassesOf[conceptId])
        {
            // a connector points to each one of its connectors
            CommonConceptGraphItem *destItem = dynamic_cast<CommonConceptGraphItem*>(validItems[superclassId]);
            if (!destItem)
                continue;
            // Omit loops
            if (srcItem == destItem)
                continue;
            // Check if there is an edgeitem of type TO which points to destItem
            bool found = false;
            auto myEdgeItems = srcItem->getEdgeItems();
            for (auto line : myEdgeItems)
            {
                if (line->getTargetItem() == destItem)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                CommonConceptGraphEdgeItem *line;
                if (srcItem->getType() == CommonConceptGraphItem::INSTANCE)
                {
                    line = new CommonConceptGraphEdgeItem(srcItem, destItem, CommonConceptGraphEdgeItem::TO, CommonConceptGraphEdgeItem::DASHED_STRAIGHT);
                } else {
                    line = new CommonConceptGraphEdgeItem(srcItem, destItem, CommonConceptGraphEdgeItem::TO, CommonConceptGraphEdgeItem::SOLID_STRAIGHT);
                }
                addItem(line);
            }
        }
    }

    // Everything which is in currentItems but not in validItems has to be removed
    // First: remove edges and free children
    QMap<UniqueId,HyperedgeItem*> toBeChecked(currentItems); // NOTE: Since we modify currentItems, we should make a snapshot of the current state
    QMap<UniqueId,HyperedgeItem*>::const_iterator it2;
    for (it2 = toBeChecked.begin(); it2 != toBeChecked.end(); ++it2)
    {
        auto id = it2.key();
        if (validItems.contains(id))
            continue;
        auto item = it2.value();
        // Remove edges
        auto edgeSet = item->getEdgeItems();
        for (auto edge : edgeSet)
        {
            edge->deregister();
            delete edge;
        }
        // Make all our children free items again!!!
        QList< QGraphicsItem* > children = item->childItems();
        for (QGraphicsItem* child : children)
        {
            CommonConceptGraphItem* commonChild = dynamic_cast<CommonConceptGraphItem*>(child);
            if (!commonChild)
                continue;
            commonChild->setParentItem(0);
            commonChild->updateEdgeItems();
        }
    }

    // Everything which is in currentItems but not in validItems has to be removed
    // Second: remove items
    for (it2 = toBeChecked.begin(); it2 != toBeChecked.end(); ++it2)
    {
        auto id = it2.key();
        if (validItems.contains(id))
            continue;
        auto item = it2.value();
        // Destruct the item
        currentItems.remove(id);
        delete item;
        //removeItem(item);
        //item->deleteLater();
    }
}

CommonConceptGraphEditor::CommonConceptGraphEditor(QWidget *parent)
: ConceptgraphEditor(parent)
{
}

CommonConceptGraphEditor::CommonConceptGraphEditor(CommonConceptGraphScene * scene, QWidget * parent)
: ConceptgraphEditor(scene, parent)
{
}

void CommonConceptGraphEditor::keyPressEvent(QKeyEvent * event)
{
    QList<CommonConceptGraphItem *> selection = scene()->selectedCommonConceptGraphItems();
    if (event->key() == Qt::Key_Delete)
    {
        for (CommonConceptGraphItem *item : selection)
        {
            // Delete edge from graph
            scene()->removeEdge(item->getHyperEdgeId());
        }
    }
    else if (event->key() == Qt::Key_Insert)
    {
        scene()->addConcept(0, "Name?");
    }
    else if (event->key() == Qt::Key_Pause)
    {
        // Toggle force based layout on or off
        ForceBasedScene *fbscene = dynamic_cast<ForceBasedScene*>(scene());
        if (fbscene)
        {
            fbscene->setLayoutEnabled(!fbscene->isLayoutEnabled());
        }
    }
    else if (event->key() == Qt::Key_F1)
    {
        // Hide/show classes
        scene()->showClasses(!scene()->classesShown());
    }
    else if (event->key() == Qt::Key_F2)
    {
        // Hide/show instances
        scene()->showInstances(!scene()->instancesShown());
    }
    else if (selection.size() && (event->key() != Qt::Key_Control))
    {
        if (!isEditLabelMode)
        {
            // Start label edit
            isEditLabelMode = true;
            currentLabel = "";
        }
        // Update current label
        if (event->key() != Qt::Key_Backspace)
        {
            currentLabel += event->text();
        } else {
            currentLabel.chop(1);
        }
        for (CommonConceptGraphItem *item : selection)
        {
            scene()->updateEdge(item->getHyperEdgeId(), currentLabel);
        }
        setDefaultLabel(currentLabel);
    }

    QGraphicsView::keyPressEvent(event);
}

void CommonConceptGraphEditor::mousePressEvent(QMouseEvent* event)
{
    QGraphicsItem *item = itemAt(event->pos());
    if (event->button() == Qt::LeftButton)
    {
        isEditLabelMode = false;
    }
    else if (event->button() == Qt::RightButton)
    {
        // If pressed on a CommonConceptGraphItem we start drawing a line
        auto edge = dynamic_cast<CommonConceptGraphItem*>(item);
        if (edge)
        {
            lineItem = new QGraphicsLineItem();
            QPen pen(Qt::red);
            lineItem->setPen(pen);
            scene()->addItem(lineItem);
            isDrawLineMode = true;
            sourceItem = edge;
        }
    }

    ConceptgraphEditor::mousePressEvent(event);
}

void CommonConceptGraphEditor::mouseReleaseEvent(QMouseEvent* event)
{
    QGraphicsItem *item = itemAt(event->pos());
    if ((event->button() == Qt::RightButton) && isDrawLineMode)
    {
        // Check if there is a CommonConceptGraphItem at current pos
        auto edge = dynamic_cast<CommonConceptGraphItem*>(item);
        if (edge)
        {
            // Add model edge
            //std::cout << "Create relation\n";
            scene()->addRelation(sourceItem->getHyperEdgeId(), edge->getHyperEdgeId(), 0, "Name?");
        }
        if (lineItem)
        {
            scene()->removeItem(lineItem);
            delete lineItem;
            lineItem = NULL;
        }
        isDrawLineMode = false;
    }

    ConceptgraphEditor::mouseReleaseEvent(event);
}

CommonConceptGraphWidget::CommonConceptGraphWidget(QWidget *parent, bool doSetup)
: ConceptgraphWidget(parent, false)
{
    if (doSetup)
    {
        // Setup my own ui
        mpNewUi = new Ui::CommonConceptGraphViewer();
        mpNewUi->setupUi(this);

        mpCommonConceptScene = new CommonConceptGraphScene();
        mpCommonConceptEditor = new CommonConceptGraphEditor(mpCommonConceptScene);
        mpScene = mpCommonConceptScene;
        mpView = mpCommonConceptEditor;
        mpView->show();
        QVBoxLayout *layout = new QVBoxLayout();
        layout->addWidget(mpView);
        mpNewUi->View->setLayout(layout);

        //mpNewUi->usageLabel->setText("LMB: Select  RMB: Associate  WHEEL: Zoom  DEL: Delete  INS: Insert  PAUSE: Toggle Layouting");
        mpNewUi->usageLabel->setText("LMB: Select  WHEEL: Zoom  DEL: Delete  PAUSE: Toggle Layouting  F1: Hide/Show Classes  F2: Hide/Show Instances");

        // Connect
        connect(mpCommonConceptScene, SIGNAL(conceptAdded(const UniqueId)), this, SLOT(onGraphChanged(const UniqueId)));
        connect(mpCommonConceptScene, SIGNAL(conceptRemoved(const UniqueId)), this, SLOT(onGraphChanged(const UniqueId)));
        connect(mpCommonConceptScene, SIGNAL(relationAdded(const UniqueId)), this, SLOT(onGraphChanged(const UniqueId)));
        connect(mpCommonConceptScene, SIGNAL(relationRemoved(const UniqueId)), this, SLOT(onGraphChanged(const UniqueId)));
    } else {
        mpNewUi = NULL;
    }
}

CommonConceptGraphWidget::~CommonConceptGraphWidget()
{
    if (mpNewUi)
        delete mpNewUi;
}

void CommonConceptGraphWidget::showEvent(QShowEvent *event)
{
    // About to be shown
    mpCommonConceptScene->setEnabled(true);
    mpCommonConceptScene->visualize();
}

void CommonConceptGraphWidget::loadFromGraph(CommonConceptGraph& graph)
{
    mpCommonConceptScene->visualize(&graph);
}

void CommonConceptGraphWidget::loadFromYAMLFile(const QString& fileName)
{
    Hypergraph* newGraph = YAML::LoadFile(fileName.toStdString()).as<Hypergraph*>(); // std::string >> YAML::Node >> Hypergraph* >> CommonConceptGraph
    Conceptgraph newCGraph(*newGraph);
    CommonConceptGraph newCCGraph(newCGraph);
    loadFromGraph(newCCGraph);
    delete newGraph;
}

void CommonConceptGraphWidget::loadFromYAML(const QString& yamlString)
{
    auto newGraph = YAML::Load(yamlString.toStdString()).as<Hypergraph*>();
    Conceptgraph newCGraph(*newGraph);
    CommonConceptGraph newCCGraph(newCGraph);
    loadFromGraph(newCCGraph);
    delete newGraph;
}

void CommonConceptGraphWidget::onGraphChanged(const UniqueId id)
{
    auto allConcepts = mpCommonConceptScene->graph()->find();
    auto allInstances = mpCommonConceptScene->graph()->instancesOf(allConcepts);
    auto allClasses   = subtract(allConcepts, allInstances);
    auto allRelations = mpCommonConceptScene->graph()->relations();
    auto allFacts = mpCommonConceptScene->graph()->factsOf(allRelations);
    auto allRelClasses = subtract(allRelations, allFacts);
    // Fill the lists
    mpNewUi->classListWidget->clear();
    for (auto classId : allClasses) mpNewUi->classListWidget->addItem(QString::fromStdString(classId));
    mpNewUi->instanceListWidget->clear();
    for (auto instanceId : allInstances) mpNewUi->instanceListWidget->addItem(QString::fromStdString(instanceId));
    mpNewUi->relationListWidget->clear();
    for (auto relationId : allRelClasses) mpNewUi->relationListWidget->addItem(QString::fromStdString(relationId));
    mpNewUi->factListWidget->clear();
    for (auto factId : allFacts) mpNewUi->factListWidget->addItem(QString::fromStdString(factId));
    // Gets triggered whenever a concept||relations has been added||removed
    mpNewUi->statsLabel->setText(
                            "CLASSES: " + QString::number(allClasses.size()) +
                            "  INSTANCES: " + QString::number(allInstances.size()) +
                            "  RELATION CLASSES: " + QString::number(allRelClasses.size()) +
                            "  FACTS: " + QString::number(allFacts.size())
                             );
}
