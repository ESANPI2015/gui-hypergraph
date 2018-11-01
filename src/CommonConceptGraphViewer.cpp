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
#include "HypergraphYAML.hpp"
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
    graph().instantiateDeepFrom(Hyperedges{superId}, label.toStdString());
    visualize();
}

void CommonConceptGraphScene::addClass(const UniqueId id, const QString& label)
{
    graph().create(id, label.toStdString());
    visualize();
}

void CommonConceptGraphScene::addFact(const UniqueId superId, const UniqueId fromId, const UniqueId toId)
{
    graph().factFrom(Hyperedges{fromId}, Hyperedges{toId}, superId);
    visualize();
}

void CommonConceptGraphScene::addRelation(const UniqueId id, const UniqueId fromId, const UniqueId toId, const QString& label)
{
    graph().relate(id, Hyperedges{fromId}, Hyperedges{toId}, label.toStdString());
    visualize();
}

void CommonConceptGraphScene::removeEdge(const UniqueId id)
{
    graph().destroy(id);
    visualize();
}

void CommonConceptGraphScene::updateEdge(const UniqueId id, const QString& label)
{
    graph().get(id)->updateLabel(label.toStdString());
    visualize();
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
    Hyperedges allConcepts(graph().find());
    Hyperedges allInstances(graph().instancesOf(allConcepts)); // offending line
    Hyperedges allClasses(subtract(allConcepts, allInstances));
    for (const UniqueId& classUID : allClasses)
        result.push_back(QString::fromStdString(classUID));
    return result;
}

QStringList CommonConceptGraphScene::getAllRelationUIDs()
{
    QStringList result;
    Hyperedges allRelClasses;
    Hyperedges allRelations(graph().relations());
    Hyperedges allFacts(graph().factsOf(allRelations));
    allRelClasses = subtract(allRelations, allFacts);
    for (UniqueId classUID : allRelClasses)
        result.push_back(QString::fromStdString(classUID));
    return result;
}

void CommonConceptGraphScene::visualize(const CommonConceptGraph& graph)
{
    // Merge & visualize
    currentCommonConceptGraph.importFrom(graph);
    visualize();
}

void CommonConceptGraphScene::visualize()
{
    // Suppress visualisation if desired
    if (!isEnabled())
        return;

    // Make a snapshot of the current graph
    CommonConceptGraph snapshot(this->graph());
    currentGraph = snapshot;

    // A: Try to draw instances/classes
    Hyperedges allConcepts(snapshot.find());
    QMap<UniqueId,CommonConceptGraphItem*> validItems;
    QMap<UniqueId, Hyperedges> superclassesOf; //TODO: Needed?
    for (UniqueId conceptId : allConcepts)
    {
        // First, try to get superclasses of an instance
        bool instance = true;
        superclassesOf[conceptId] = snapshot.instancesOf(Hyperedges{conceptId},"", CommonConceptGraph::TraversalDirection::FORWARD);
        if (!superclassesOf[conceptId].size())
        {
            // Get the superclasses of a class
            instance = false;
            superclassesOf[conceptId] = snapshot.subclassesOf(Hyperedges{conceptId},"",CommonConceptGraph::TraversalDirection::FORWARD);
        }

        // Skip if we shall not draw it
        if (instance && !mShowInstances)
            continue;
        if (!instance && !mShowClasses)
            continue;

        // Create or get item
        CommonConceptGraphItem *item;
        if (!currentItems.contains(conceptId))
        {
            // Check if the concept is a class or an instance
            if (instance)
            {
                item = new CommonConceptGraphItem(snapshot.get(conceptId), CommonConceptGraphItem::INSTANCE, "NO LABEL");
            } else {
                item = new CommonConceptGraphItem(snapshot.get(conceptId), CommonConceptGraphItem::CLASS, "NO LABEL");
            }
            addItem(item);
            currentItems[conceptId] = item;
        } else {
            item = dynamic_cast<CommonConceptGraphItem*>(currentItems[conceptId]);
        }
        // Create superclass label
        std::string superclassLabel;
        for (auto superclassId : superclassesOf[conceptId])
        {
            superclassLabel += (" " + snapshot.get(superclassId)->label());
        }
        // Make sure that the labels are up-to-date
        item->setLabel(QString::fromStdString(snapshot.get(conceptId)->label()), QString::fromStdString(superclassLabel));
        validItems[conceptId] = item;
    }

    // B: Interpret relations. That means that we have to create different edges for different types
    // Cycle through visible concepts
    for (UniqueId conceptId : allConcepts)
    {
        // Skip invalid items
        if (!validItems.contains(conceptId))
            continue;
        CommonConceptGraphItem *srcItem(dynamic_cast<CommonConceptGraphItem*>(validItems[conceptId]));
        if (!srcItem)
            continue;
        Hyperedges relationsFrom(snapshot.relationsFrom(Hyperedges{conceptId}));

        // Cycle through possible counterparts
        for (UniqueId otherId : allConcepts)
        {
            // Omit loops
            if (conceptId == otherId)
                continue;
            // Skip invalid items
            if (!validItems.contains(otherId))
                continue;
            CommonConceptGraphItem *destItem(dynamic_cast<CommonConceptGraphItem*>(validItems[otherId]));
            if (!destItem)
                continue;
            Hyperedges relationsTo(snapshot.relationsTo(Hyperedges{otherId}));
            // The intersection between relationsFrom and relationsTo are all relations between the two concepts
            Hyperedges commonRelations(intersect(relationsFrom, relationsTo));
            for (UniqueId relId : commonRelations)
            {
                // Get super relation(s) of a fact
                bool fact = true;
                Hyperedges superRelations(snapshot.factsOf(relId, "", CommonConceptGraph::TraversalDirection::FORWARD));
                if (!superRelations.size())
                {
                    fact = false;
                    superRelations = snapshot.subrelationsOf(relId, "", CommonConceptGraph::TraversalDirection::FORWARD);
                }
                // Now we know if a relation is a fact of some relation def or the def itself
                // We won't show defs, so we skip it
                if (!fact)
                    continue;

                // Extend superRelations by transitive closure
                // Only then we can be sure that CommonConceptGraph relations are visible
                superRelations = unite(superRelations, snapshot.subrelationsOf(superRelations, "", CommonConceptGraph::TraversalDirection::FORWARD));

                // Now we want to make the fact visible
                // We have to change style and/or color of the link depending on superclass
                // Or we interpret the edge (like parent-child relationship)
                if (std::find(superRelations.begin(), superRelations.end(), CommonConceptGraph::HasAId) != superRelations.end())
                {
                    // parent -- HAS-A --> child
                    // Check if this item is already part of the parent
                    if (!srcItem->isAncestorOf(destItem))
                    {
                        // Be sure that destItem does not yet have a parent
                        if (!destItem->parentItem())
                            destItem->setParentItem(srcItem);
                    }
                }
                else if (std::find(superRelations.begin(), superRelations.end(), CommonConceptGraph::PartOfId) != superRelations.end())
                {
                    // part -- PART-OF --> whole
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
                        auto line = new CommonConceptGraphEdgeItem(srcItem, destItem, CommonConceptGraphEdgeItem::TO, CommonConceptGraphEdgeItem::DOTTED_STRAIGHT);
                        addItem(line);
                    }
                }
                else if (std::find(superRelations.begin(), superRelations.end(), CommonConceptGraph::IsAId) != superRelations.end())
                {
                    // subclass -- IS-A --> superclass
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
                        auto line = new CommonConceptGraphEdgeItem(srcItem, destItem, CommonConceptGraphEdgeItem::TO, CommonConceptGraphEdgeItem::SOLID_STRAIGHT);
                        addItem(line);
                    }
                }
                else if (std::find(superRelations.begin(), superRelations.end(), CommonConceptGraph::InstanceOfId) != superRelations.end())
                {
                    // individual -- INSTANCE-OF --> superclass
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
                        auto line = new CommonConceptGraphEdgeItem(srcItem, destItem, CommonConceptGraphEdgeItem::TO, CommonConceptGraphEdgeItem::DASHED_STRAIGHT);
                        addItem(line);
                    }
                }
                else if (std::find(superRelations.begin(), superRelations.end(), CommonConceptGraph::ConnectsId) != superRelations.end())
                {
                    // interface -- CONNECTS --> interface
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
        removeItem(item);
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

void CommonConceptGraphWidget::loadFromGraph(const CommonConceptGraph& graph)
{
    mpCommonConceptScene->visualize(graph);
}

void CommonConceptGraphWidget::loadFromYAMLFile(const QString& fileName)
{
    loadFromGraph(CommonConceptGraph(YAML::LoadFile(fileName.toStdString()).as<Hypergraph>()));
}

void CommonConceptGraphWidget::loadFromYAML(const QString& yamlString)
{
    loadFromGraph(CommonConceptGraph(YAML::Load(yamlString.toStdString()).as<Hypergraph>()));
}

void CommonConceptGraphWidget::onGraphChanged(QGraphicsItem* item)
{
    HyperedgeItem *hitem(dynamic_cast< HyperedgeItem *>(item));
    if (!hitem)
        return;
    // Get current scene pos of view
    QPointF centerOfView(mpCommonConceptEditor->mapToScene(mpCommonConceptEditor->viewport()->rect().center()));
    QPointF noise(qrand() % 100, qrand() % 100);
    item->setPos(centerOfView + noise);
}

void CommonConceptGraphWidget::onGraphChanged(const UniqueId id)
{
    Hyperedges allConcepts(mpCommonConceptScene->graph().find());
    Hyperedges allInstances(mpCommonConceptScene->graph().instancesOf(allConcepts));
    Hyperedges allClasses(subtract(allConcepts, allInstances));
    Hyperedges allRelations(mpCommonConceptScene->graph().relations());
    Hyperedges allFacts(mpCommonConceptScene->graph().factsOf(allRelations));
    Hyperedges allRelClasses(subtract(allRelations, allFacts));
    // Fill the lists
    mpNewUi->classListWidget->clear();
    for (const UniqueId& classId : allClasses) mpNewUi->classListWidget->addItem(QString::fromStdString(classId));
    mpNewUi->instanceListWidget->clear();
    for (const UniqueId& instanceId : allInstances) mpNewUi->instanceListWidget->addItem(QString::fromStdString(instanceId));
    mpNewUi->relationListWidget->clear();
    for (const UniqueId& relationId : allRelClasses) mpNewUi->relationListWidget->addItem(QString::fromStdString(relationId));
    mpNewUi->factListWidget->clear();
    for (const UniqueId& factId : allFacts) mpNewUi->factListWidget->addItem(QString::fromStdString(factId));
    // Gets triggered whenever a concept||relations has been added||removed
    mpNewUi->statsLabel->setText(
                            "CLASSES: " + QString::number(allClasses.size()) +
                            "  INSTANCES: " + QString::number(allInstances.size()) +
                            "  RELATION CLASSES: " + QString::number(allRelClasses.size()) +
                            "  FACTS: " + QString::number(allFacts.size())
                             );
}
