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
    mShowClasses = true;
    mShowInstances = true;
    mpUpdateTimer = new QTimer(this);
    connect(mpUpdateTimer, SIGNAL(timeout()), this, SLOT(updateVisualization()));
    mpUpdateTimer->start(200);
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
    Hyperedges instances(graph().instantiateFrom(Hyperedges{superId}, label.toStdString()));
    for (const UniqueId& id : instances)
        visualize(id);
}

void CommonConceptGraphScene::addClass(const UniqueId id, const QString& label)
{
    graph().concept(id, label.toStdString());
    visualize(id);
}

void CommonConceptGraphScene::addFact(const UniqueId superId, const UniqueId fromId, const UniqueId toId)
{
    Hyperedges facts(graph().factFrom(Hyperedges{fromId}, Hyperedges{toId}, superId));
    visualize(fromId);
    visualize(toId);
}

void CommonConceptGraphScene::addRelation(const UniqueId id, const UniqueId fromId, const UniqueId toId, const QString& label)
{
    graph().relate(id, Hyperedges{fromId}, Hyperedges{toId}, label.toStdString());
    visualize(fromId);
    visualize(toId);
}

void CommonConceptGraphScene::removeEdge(const UniqueId id)
{
    if (graph().exists(id))
        graph().destroy(id);
    visualize(id);
}

void CommonConceptGraphScene::updateEdge(const UniqueId id, const QString& label)
{
    graph().access(id).updateLabel(label.toStdString());
    visualize(id);
}

void CommonConceptGraphScene::showClasses(const bool value)
{
    mShowClasses = value;
}

void CommonConceptGraphScene::showInstances(const bool value)
{
    mShowInstances = value;
}

QStringList CommonConceptGraphScene::getAllClassUIDs()
{
    QStringList result;
    Hyperedges allConcepts(graph().concepts());
    Hyperedges allInstances(graph().instancesOf(allConcepts));
    Hyperedges allClasses(subtract(allConcepts, allInstances));
    for (const UniqueId& classUID : allClasses)
        result.push_back(QString::fromStdString(classUID));
    return result;
}

QStringList CommonConceptGraphScene::getAllRelationUIDs()
{
    QStringList result;
    Hyperedges allRelations(graph().relations());
    Hyperedges allFacts(graph().factsOf(allRelations));
    Hyperedges allRelClasses(subtract(subtract(allRelations, allFacts), graph().access(CommonConceptGraph::FactOfId).pointingFrom()));
    for (const UniqueId& classUID : allRelClasses)
        result.push_back(QString::fromStdString(classUID));
    return result;
}

void CommonConceptGraphScene::visualize(const CommonConceptGraph& graph)
{
    // Merge & visualize
    currentCommonConceptGraph.importFrom(graph);
}

void CommonConceptGraphScene::updateVisualization()
{
    visualize();
    currentGraph = this->graph();
}

void CommonConceptGraphScene::visualize(const UniqueId& updatedId)
{
    // TODO: If something is an instance and we want to hide it, make it invisible (also for classes)
    // Suppress visualisation if desired
    if (!isEnabled())
        return;

    Hyperedges allConcepts(this->graph().concepts());
    if (!allConcepts.size())
        return;

    static UniqueId conceptId(allConcepts[0]);

    // If we got a HINT on what to update, we do so
    if (!updatedId.empty())
        conceptId = updatedId;

    // Find the current concept in graph
    if (!this->graph().exists(conceptId))
    {
        // Delete CommonConceptGraphItem if it (still) exists
        if (currentItems.contains(conceptId))
        {
            CommonConceptGraphItem *toDelete(dynamic_cast<CommonConceptGraphItem*>(currentItems[conceptId]));
            // Check children: Put ownership back to scene
            QList< QGraphicsItem* > children = toDelete->childItems();
            for (QGraphicsItem* child : children)
            {
                child->setParentItem(0);
            }
            // Remove edges: Disconnect first, then delete
            auto edgeSet = toDelete->getEdgeItems();
            for (auto edge : edgeSet)
            {
                edge->deregister();
            }
            for (auto edge : edgeSet)
            {
                delete edge;
            }
            // Destruct the item
            currentItems.remove(conceptId);
            removeItem(toDelete);
            delete toDelete;
        }

        // In the following, start from the beginning
        conceptId = allConcepts[0];
    }
    
    // First, try to get superclasses of an instance
    bool instance = true;
    Hyperedges superclassesOf(this->graph().instancesOf(Hyperedges{conceptId},"", CommonConceptGraph::TraversalDirection::FORWARD));
    if (!superclassesOf.size())
    {
        // Get the superclasses of a class
        instance = false;
        superclassesOf = this->graph().subclassesOf(Hyperedges{conceptId},"",CommonConceptGraph::TraversalDirection::FORWARD);
    }

    // Check if the concept exists already
    CommonConceptGraphItem *item(NULL);
    if (!currentItems.contains(conceptId))
    {
        // Check if the concept is a class or an instance
        if (instance)
        {
            item = new CommonConceptGraphItem(conceptId, CommonConceptGraphItem::INSTANCE, this->graph().access(conceptId).label());
        } else {
            item = new CommonConceptGraphItem(conceptId, CommonConceptGraphItem::CLASS, this->graph().access(conceptId).label());
        }
        addItem(item);
        currentItems[conceptId] = item;
    } else {
        // Delete the item if desired
        item = dynamic_cast<CommonConceptGraphItem*>(currentItems[conceptId]);
    }

    // Set visibility
    if (instance)
    {
        item->setVisible(mShowInstances);
    }
    if (!instance)
    {
        item->setVisible(mShowClasses);
    }

    // Update label
    std::string superclassLabel;
    for (const UniqueId& superclassId : superclassesOf)
    {
        superclassLabel += (" " + this->graph().access(superclassId).label());
    }
    item->setLabel(QString::fromStdString(this->graph().access(conceptId).label()), QString::fromStdString(superclassLabel));

    // Update relations
    Hyperedges relationsFrom(this->graph().relationsFrom(Hyperedges{conceptId}));
    CommonConceptGraphItem* srcItem(item);
    auto myEdgeItems = srcItem->getEdgeItems();
    Hyperedges otherIds(this->graph().isPointingTo(relationsFrom));
    for (const UniqueId& otherId : otherIds)
    {
        // Skip invalid items
        if (!currentItems.contains(otherId))
            continue;
        CommonConceptGraphItem *destItem(dynamic_cast<CommonConceptGraphItem*>(currentItems[otherId]));
        if (!destItem)
            continue;

        // Check if there is an edgeitem which points to destItem
        // Also: if either src or target item are invisible set edge items to invisible as well
        bool found = false;
        for (auto line : myEdgeItems)
        {
            if (line->getTargetItem() == destItem)
            {
                found = true;
                break;
            }
        }

        Hyperedges relationsTo(this->graph().relationsTo(Hyperedges{otherId}));
        // The intersection between relationsFrom and relationsTo are all relations between the two concepts
        Hyperedges commonRelations(intersect(relationsFrom, relationsTo));
        for (const UniqueId& relId : commonRelations)
        {
            // Get super relation(s) of a fact
            Hyperedges superRelations(this->graph().factsOf(relId, Hyperedges(), Hyperedges(), CommonConceptGraph::TraversalDirection::FORWARD));
            if (!superRelations.size())
            {
                continue;
            }

            // Extend superRelations by transitive closure
            // Only then we can be sure that CommonConceptGraph relations are visible
            superRelations = unite(superRelations, this->graph().subrelationsOf(superRelations, "", CommonConceptGraph::TraversalDirection::FORWARD));

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

            // For the others below, we skip everything if there is already an edge item between two concepts
            if (found)
                continue;

            if (std::find(superRelations.begin(), superRelations.end(), CommonConceptGraph::PartOfId) != superRelations.end())
            {
                // part -- PART-OF --> whole
                auto line = new CommonConceptGraphEdgeItem(srcItem, destItem, CommonConceptGraphEdgeItem::TO, CommonConceptGraphEdgeItem::DOTTED_STRAIGHT);
                addItem(line);
                continue;
            }

            if (std::find(superRelations.begin(), superRelations.end(), CommonConceptGraph::IsAId) != superRelations.end())
            {
                // subclass -- IS-A --> superclass
                auto line = new CommonConceptGraphEdgeItem(srcItem, destItem, CommonConceptGraphEdgeItem::TO, CommonConceptGraphEdgeItem::SOLID_STRAIGHT);
                addItem(line);
                continue;
            }

            if (std::find(superRelations.begin(), superRelations.end(), CommonConceptGraph::InstanceOfId) != superRelations.end())
            {
                // individual -- INSTANCE-OF --> superclass
                auto line = new CommonConceptGraphEdgeItem(srcItem, destItem, CommonConceptGraphEdgeItem::TO, CommonConceptGraphEdgeItem::DASHED_STRAIGHT);
                addItem(line);
                continue;
            }

            if (std::find(superRelations.begin(), superRelations.end(), CommonConceptGraph::ConnectsId) != superRelations.end())
            {
                // interface -- CONNECTS --> interface
                auto line = new CommonConceptGraphEdgeItem(srcItem, destItem, CommonConceptGraphEdgeItem::TO, CommonConceptGraphEdgeItem::SOLID_CURVED);
                addItem(line);
                continue;
            }
        }
    }

    // Make edges visible or invisible
    myEdgeItems = srcItem->getEdgeItems();
    for (auto line : myEdgeItems)
    {
        line->setVisible(srcItem->isVisible() && line->getTargetItem()->isVisible());
    }

    // Last step: Determine next concept
    Hyperedges::const_iterator it(std::find(allConcepts.begin(), allConcepts.end(), conceptId));
    it++;
    if (it != allConcepts.end())
    {
        conceptId = *it;
    } else {
        conceptId = allConcepts[0];
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
    // TODO: Use UID to update only PARTS of the GUI!
    Hyperedges allConcepts(mpCommonConceptScene->graph().concepts());
    Hyperedges allInstances(mpCommonConceptScene->graph().instancesOf(allConcepts));
    Hyperedges allClasses(subtract(allConcepts, allInstances));
    Hyperedges allRelations(mpCommonConceptScene->graph().relations());
    Hyperedges allFacts(mpCommonConceptScene->graph().factsOf(allRelations));
    Hyperedges allRelClasses(subtract(subtract(allRelations, allFacts), mpCommonConceptScene->graph().access(CommonConceptGraph::FactOfId).pointingFrom()));
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
