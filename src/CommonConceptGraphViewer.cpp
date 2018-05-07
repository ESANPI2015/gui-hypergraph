#include "CommonConceptGraphViewer.hpp"
#include "ui_CommonConceptGraphViewer.h"
#include "ui_HypergraphViewer.h"
#include "CommonConceptGraphItem.hpp"

#include <QGraphicsScene>
#include <QWheelEvent>
#include <QTimer>
#include <QtCore>
#include <QInputDialog>

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
        if (edge->getType() == CommonConceptGraphItem::CommonConceptGraphItemType::INSTANCE)
            emit instanceAdded(edge->getHyperEdgeId());
        else
            emit classAdded(edge->getHyperEdgeId());
    }
}

void CommonConceptGraphScene::removeItem(QGraphicsItem *item)
{
    // Emit signals
    CommonConceptGraphItem *edge = dynamic_cast<CommonConceptGraphItem*>(item);
    if (edge)
    {
        if (edge->getType() == CommonConceptGraphItem::CommonConceptGraphItemType::INSTANCE)
            emit instanceRemoved(edge->getHyperEdgeId());
        else
            emit classRemoved(edge->getHyperEdgeId());
    }

    ConceptgraphScene::removeItem(item);
}

void CommonConceptGraphScene::addInstance(const UniqueId superId, const QString& label)
{
    CommonConceptGraph *g = graph();
    if (g)
    {
        g->instantiateFrom(superId, label.toStdString());
        visualize();
    }
}

void CommonConceptGraphScene::addClass(const UniqueId id, const QString& label)
{
    CommonConceptGraph *g = graph();
    if (g)
    {
        g->create(id, label.toStdString());
        visualize();
    }
}

void CommonConceptGraphScene::addFact(const UniqueId superId, const UniqueId fromId, const UniqueId toId)
{
    CommonConceptGraph *g = graph();
    if (g)
    {
        g->factFrom(Hyperedges{fromId}, Hyperedges{toId}, superId);
        visualize();
    }
}

void CommonConceptGraphScene::addRelation(const UniqueId id, const UniqueId fromId, const UniqueId toId, const QString& label)
{
    CommonConceptGraph *g = graph();
    if (g)
    {
        g->relate(id, Hyperedges{fromId}, Hyperedges{toId}, label.toStdString());
        visualize();
    }
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

QStringList CommonConceptGraphScene::getAllClassUIDs()
{
    QStringList result;
    Hyperedges allClasses;
    CommonConceptGraph *g = graph();
    if (g)
    {
        Hyperedges allConcepts = g->find();
        Hyperedges allInstances = g->instancesOf(allConcepts);
        allClasses = subtract(allConcepts, allInstances);
    }
    for (UniqueId classUID : allClasses)
        result.push_back(QString::fromStdString(classUID));
    return result;
}

QStringList CommonConceptGraphScene::getAllRelationUIDs()
{
    QStringList result;
    Hyperedges allRelClasses;
    CommonConceptGraph *g = graph();
    if (g)
    {
        Hyperedges allRelations = g->relations();
        Hyperedges allFacts = g->factsOf(allRelations);
        allRelClasses = subtract(allRelations, allFacts);
    }
    for (UniqueId classUID : allRelClasses)
        result.push_back(QString::fromStdString(classUID));
    return result;
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

    // Make a snapshot of the current graph
    CommonConceptGraph snapshot(*this->graph());

    // Select either instances or classes
    auto allConcepts = snapshot.find();
    auto allInstances = snapshot.instancesOf(allConcepts);
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
        Hyperedges children = snapshot.childrenOf(Hyperedges{conceptId});
        children.erase(conceptId);
        childrenOfParent[conceptId] = children;
        // Find parts
        Hyperedges parts = snapshot.componentsOf(Hyperedges{conceptId});
        parts.erase(conceptId);
        partsOfWhole[conceptId] = parts;
        // Find endpoints
        Hyperedges endpoints = snapshot.endpointsOf(Hyperedges{conceptId});
        endpoints.erase(conceptId);
        endpointsOfConnector[conceptId] = endpoints;
    }
    // For classes
    for (auto classId : allClasses)
    {
        // Find superclasses
        Hyperedges superclasses = snapshot.subclassesOf(Hyperedges{classId},"",CommonConceptGraph::TraversalDirection::DOWN);
        superclasses.erase(classId);
        superclassesOf[classId] = superclasses;
    }
    // For instances
    for (auto instanceId : allInstances)
    {
        // Find superclasses
        Hyperedges superclasses = snapshot.instancesOf(Hyperedges{instanceId},"",CommonConceptGraph::TraversalDirection::DOWN);
        superclasses.erase(instanceId);
        superclassesOf[instanceId] = superclasses;
    }

    // Second pass: Draw either class or instance
    QMap<UniqueId,CommonConceptGraphItem*> validItems;
    for (auto conceptId : allConcepts)
    {
        // Create or get item
        CommonConceptGraphItem *item;
        // Create superclass label
        std::string superclassLabel;
        for (auto superclassId : superclassesOf[conceptId])
        {
            superclassLabel += (" " + snapshot.get(superclassId)->label());
        }
        if (!currentItems.contains(conceptId))
        {
            // Check if the concept is a class or an instance
            if (allInstances.count(conceptId))
            {
                item = new CommonConceptGraphItem(snapshot.get(conceptId), CommonConceptGraphItem::INSTANCE, superclassLabel);
            } else {
                item = new CommonConceptGraphItem(snapshot.get(conceptId), CommonConceptGraphItem::CLASS, superclassLabel);
            }
            addItem(item);
            currentItems[conceptId] = item;
        } else {
            item = dynamic_cast<CommonConceptGraphItem*>(currentItems[conceptId]);
        }
        // Make sure that the labels are up-to-date
        item->setLabel(QString::fromStdString(snapshot.get(conceptId)->label()), QString::fromStdString(superclassLabel));
        validItems[conceptId] = item;
    }

    // Third pass: Interpret common relationships
    for (auto conceptId : allConcepts)
    {
        // Check if valid
        if (!validItems.contains(conceptId))
            continue;
        CommonConceptGraphItem *srcItem = dynamic_cast<CommonConceptGraphItem*>(validItems[conceptId]);
        if (!srcItem)
            continue;
        // Every child shall be attached to its parent
        for (auto childId : childrenOfParent[conceptId])
        {
            // Check if valid
            if (!validItems.contains(childId))
                continue;
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
                // Be sure that destItem does not yet have a parent
                if (!destItem->parentItem())
                    destItem->setParentItem(srcItem);
            }
        }
        // Every part shall be linked to its container/whole
        for (auto partId : partsOfWhole[conceptId])
        {
            // Check if valid
            if (!validItems.contains(partId))
                continue;
            // a part points to its whole
            CommonConceptGraphItem *destItem = dynamic_cast<CommonConceptGraphItem*>(validItems[partId]);
            if (!destItem)
                continue;
            // Omit loops
            if (srcItem == destItem)
                continue;
            // Check if there is an edgeitem of type TO which points to destItem
            bool found = false;
            auto myEdgeItems = destItem->getEdgeItems();
            for (auto line : myEdgeItems)
            {
                if (line->getTargetItem() == srcItem)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                // srcItem/whole <-- PART-OF -- destItem/part
                auto line = new CommonConceptGraphEdgeItem(destItem, srcItem, CommonConceptGraphEdgeItem::TO, CommonConceptGraphEdgeItem::DOTTED_STRAIGHT);
                addItem(line);
            }
        }
        // Every connector shall be wired to its endpoints
        for (auto endpointId : endpointsOfConnector[conceptId])
        {
            // Check if valid
            if (!validItems.contains(endpointId))
                continue;
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
            // Check if valid
            if (!validItems.contains(superclassId))
                continue;
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
    QList< QGraphicsItem* > toBeDeleted;
    QList< QGraphicsItem* >::const_iterator dit;
    QMap<UniqueId,HyperedgeItem*> toBeChecked(currentItems); // NOTE: Since we modify currentItems, we should make a snapshot of the current state
    QMap<UniqueId,HyperedgeItem*>::const_iterator it2;
    // First pass: Remove all children
    for (it2 = toBeChecked.begin(); it2 != toBeChecked.end(); ++it2)
    {
        auto id = it2.key();
        if (validItems.contains(id))
            continue;
        auto item = it2.value();
        // Check children: Put ownership back to scene
        QList< QGraphicsItem* > children = item->childItems();
        for (QGraphicsItem* child : children)
        {
            child->setParentItem(0);
        }
    }
    // Third: List items to be removed
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
            toBeDeleted.push_back(edge);
        }
        // Destruct the item
        currentItems.remove(id);
        toBeDeleted.push_back(item);
        //removeItem(item); // FIXME: Causes SEGFAULT when having child items?
    }

    // Finally: Delete items
    for (dit = toBeDeleted.begin(); dit != toBeDeleted.end(); ++dit)
    {
        QGraphicsItem* item(*dit);
        delete item;
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
        // First question: CLASS or INSTANCE?
        bool ok;
        QStringList items;
        items << tr("INSTANCE") << tr("CLASS");
        QString which = QInputDialog::getItem(this, tr("New Concept"), tr("Select if it is a"), items, 0, false, &ok);
        if (ok && !which.isEmpty())
        {
            if (which == tr("INSTANCE"))
            {
                // Instance! Ask for class.
                items.clear();
                items = scene()->getAllClassUIDs();
                QString classUid = QInputDialog::getItem(this, tr("New Instance"), tr("Of class:"), items, 0, false, &ok);
                if (ok && !classUid.isEmpty())
                {
                    scene()->addInstance(classUid.toStdString(), currentLabel);
                }
            } else {
                // Class! Ask for UID
                QString classUid = QInputDialog::getText(this, tr("New Class"), tr("Unqiue Identifier:"), QLineEdit::Normal, tr("SomeUID"), &ok);
                if (ok && !classUid.isEmpty())
                {
                    scene()->addClass(classUid.toStdString(), currentLabel);
                }
            }
        }
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
            // First question: RELATION DEF or FACT
            bool ok;
            QStringList items;
            items << tr("FACT") << tr("RELATION DEF");
            QString which = QInputDialog::getItem(this, tr("New Relation"), tr("Select if it is a"), items, 0, false, &ok);
            if (ok && !which.isEmpty())
            {
                if (which == tr("FACT"))
                {
                    // FACT! Ask for class.
                    items.clear();
                    items = scene()->getAllRelationUIDs();
                    QString classUid = QInputDialog::getItem(this, tr("New Fact"), tr("Of relation:"), items, 0, false, &ok);
                    if (ok && !classUid.isEmpty())
                    {
                        scene()->addFact(classUid.toStdString(), sourceItem->getHyperEdgeId(), edge->getHyperEdgeId());
                    }
                } else {
                    // RELATION DEF! Ask for UID
                    QString classUid = QInputDialog::getText(this, tr("New Relation Definition"), tr("Unqiue Identifier:"), QLineEdit::Normal, tr("SomeUID"), &ok);
                    if (ok && !classUid.isEmpty())
                    {
                        scene()->addRelation(classUid.toStdString(), sourceItem->getHyperEdgeId(), edge->getHyperEdgeId(), classUid);
                    }
                }
            }
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
        connect(mpCommonConceptScene, SIGNAL(itemAdded(QGraphicsItem*)), this, SLOT(onGraphChanged(QGraphicsItem*)));
        connect(mpCommonConceptScene, SIGNAL(instanceAdded(const UniqueId)), this, SLOT(onGraphChanged(const UniqueId)));
        connect(mpCommonConceptScene, SIGNAL(classAdded(const UniqueId)), this, SLOT(onGraphChanged(const UniqueId)));
        connect(mpCommonConceptScene, SIGNAL(instanceRemoved(const UniqueId)), this, SLOT(onGraphChanged(const UniqueId)));
        connect(mpCommonConceptScene, SIGNAL(classRemoved(const UniqueId)), this, SLOT(onGraphChanged(const UniqueId)));
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

void CommonConceptGraphWidget::onGraphChanged(QGraphicsItem* item)
{
    HyperedgeItem *hitem(dynamic_cast< HyperedgeItem *>(item));
    if (!hitem)
        return;
    // Get current scene pos of view
    QPointF centerOfView(mpCommonConceptEditor->mapToScene(mpCommonConceptEditor->viewport()->rect().center()));
    QPointF noise(qrand() % 100 - 50, qrand() % 100 - 50);
    item->setPos(centerOfView + noise);
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
