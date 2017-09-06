#include "CommonConceptGraphViewer.hpp"
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
        //if (edge->getType() != CommonConceptGraphItem::RELATION)
        //    emit conceptAdded(edge->getHyperEdgeId());
        //else
        //    emit relationAdded(edge->getHyperEdgeId());
    }
}

void CommonConceptGraphScene::removeItem(QGraphicsItem *item)
{
    // Emit signals
    CommonConceptGraphItem *edge = dynamic_cast<CommonConceptGraphItem*>(item);
    if (edge)
    {
        //if (edge->getType() != CommonConceptGraphItem::RELATION)
        //    emit conceptRemoved(edge->getHyperEdgeId());
        //else
        //    emit relationRemoved(edge->getHyperEdgeId());
    }

    ConceptgraphScene::removeItem(item);
}

void CommonConceptGraphScene::addConcept(const unsigned id, const QString& label)
{
    //CommonConceptGraph *g = graph();
    //if (g)
    //{
    //    unsigned theId = id > 0 ? id : qHash(label);
    //    while (g->create(theId, label.toStdString()).empty()) theId++;
    //    visualize();
    //}
}

void CommonConceptGraphScene::addRelation(const unsigned fromId, const unsigned toId, const unsigned id, const QString& label)
{
    //CommonConceptGraph *g = graph();
    //if (g)
    //{
    //    unsigned theId = id > 0 ? id : qHash(label);
    //    while (g->relate(theId, fromId, toId, label.toStdString()).empty()) theId++;
    //    visualize();
    //}
}

void CommonConceptGraphScene::removeEdge(const unsigned id)
{
    CommonConceptGraph *g = graph();
    if (g)
    {
        g->destroy(id);
        visualize();
    }
}

void CommonConceptGraphScene::updateEdge(const unsigned int id, const QString& label)
{
    //CommonConceptGraph *g = graph();
    //if (g)
    //{
    //    g->get(id)->updateLabel(label.toStdString());
    //    visualize();
    //}
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

    // TODO: Maybe we want to have a switch to either show CLASSES and FACTS between CLASSES or
    // We want to visualize only INSTANCES and some FACTS between them
    auto allInstances = this->graph()->instancesOf(this->graph()->find());

    // First pass: Sort instances into different sets
    QMap< unsigned int, Hyperedges > childrenOfParent;
    QMap< unsigned int, Hyperedges > parentsOfChild;
    QMap< unsigned int, Hyperedges > partsOfWhole;
    QMap< unsigned int, Hyperedges > superclassesOfInst;
    QMap< unsigned int, Hyperedges > endpointsOfConnector;;
    for (auto instanceId : allInstances)
    {
        //std::cout << "Instance " << this->graph()->get(instanceId)->label() << "\n";
        Hyperedges children = this->graph()->childrenOf(Hyperedges{instanceId});
        children.erase(instanceId);
        childrenOfParent[instanceId] = children;
        //std::cout << "\t#Children " << children.size() << "\n";
        Hyperedges parents = this->graph()->childrenOf(Hyperedges{instanceId},"",CommonConceptGraph::TraversalDirection::UP);
        parents.erase(instanceId);
        parentsOfChild[instanceId] = parents;
        //std::cout << "\t#Parents " << parents.size() << "\n";
        Hyperedges parts = this->graph()->partsOf(Hyperedges{instanceId});
        parts.erase(instanceId);
        partsOfWhole[instanceId] = parts;
        //std::cout << "\t#Parts " << parts.size() << "\n";
        Hyperedges superclasses = this->graph()->instancesOf(Hyperedges{instanceId},"",CommonConceptGraph::TraversalDirection::DOWN);
        superclasses.erase(instanceId);
        superclassesOfInst[instanceId] = superclasses;
        //std::cout << "\t#Superclasses " << superclasses.size() << "\n";
        Hyperedges endpoints = this->graph()->endpointsOf(Hyperedges{instanceId});
        endpoints.erase(instanceId);
        endpointsOfConnector[instanceId] = endpoints;
        //std::cout << "\t#Endpoints " << endpoints.size() << "\n";
    }

    // Remove parts from allInstances
    QMap< unsigned int, Hyperedges >::const_iterator it;
    for (it = partsOfWhole.begin(); it != partsOfWhole.end(); ++it)
    {
        allInstances = subtract(allInstances, it.value());
    }

    // Remove all children of parents which are not instances
    for (it = parentsOfChild.begin(); it != parentsOfChild.end(); ++it)
    {
        // Not a child at all? continue
        if (!it.value().size())
            continue;
        // At least one of the parents is an instance? continue
        if (intersect(it.value(), allInstances).size())
            continue;
        // Remove child of non-instance
        allInstances.erase(it.key());
    }

    int N = allInstances.size();
    int dim = N * mEquilibriumDistance / 2;
    //std::cout << "#Instances\\Parts: " << N << "\n";

    // Second pass: Draw all instances except the parts
    QMap<unsigned int,CommonConceptGraphItem*> validItems;
    for (auto instanceId : allInstances)
    {
        // Create or get item
        CommonConceptGraphItem *item;
        if (!currentItems.contains(instanceId))
        {
            // Create superclass label
            std::string superclassLabel;
            for (auto superclassId : superclassesOfInst[instanceId])
            {
                superclassLabel += (" " + this->graph()->get(superclassId)->label());
            }
            // Check if it is a connector or a normal thing
            if (endpointsOfConnector[instanceId].size())
            {
                item = new CommonConceptGraphItem(this->graph()->get(instanceId), CommonConceptGraphItem::CONNECTOR, superclassLabel);
            } else {
                item = new CommonConceptGraphItem(this->graph()->get(instanceId), CommonConceptGraphItem::NORMAL, superclassLabel);
            }
            item->setPos(qrand() % dim - dim/2, qrand() % dim - dim/2);
            addItem(item);
            currentItems[instanceId] = item;
        } else {
            item = dynamic_cast<CommonConceptGraphItem*>(currentItems[instanceId]);
        }
        validItems[instanceId] = item;
    }

    // Third pass:
    // Every connector shall be wired to its endpoints
    // Every child shall be attached to its parent
    for (auto instanceId : allInstances)
    {
        CommonConceptGraphItem *srcItem = dynamic_cast<CommonConceptGraphItem*>(validItems[instanceId]);
        for (auto childId : childrenOfParent[instanceId])
        {
            // A child is in validItems, currentItems and visible ... But we want it to be part of its parent
            CommonConceptGraphItem *destItem = dynamic_cast<CommonConceptGraphItem*>(validItems[childId]);
            // Omit loops
            if (srcItem == destItem)
                continue;
            // Check if this item is already part of the parent
            if (!srcItem->isAncestorOf(destItem))
            {
                destItem->setParentItem(srcItem);
            }
        }
        for (auto endpointId : endpointsOfConnector[instanceId])
        {
            // a connector points to each one of its connectors
            CommonConceptGraphItem *destItem = dynamic_cast<CommonConceptGraphItem*>(validItems[endpointId]);
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
                auto line = new EdgeItem(srcItem, destItem);
                addItem(line);
            }
        }
    }

    // Everything which is in currentItems but not in validItems has to be removed
    // First: remove edges and free children
    QMap<unsigned int,HyperedgeItem*> toBeChecked(currentItems); // NOTE: Since we modify currentItems, we should make a snapshot of the current state
    QMap<unsigned int,HyperedgeItem*>::const_iterator it2;
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

CommonConceptGraphWidget::CommonConceptGraphWidget(QWidget *parent)
: ConceptgraphWidget(parent)
{
    // The base class constructor created a pair of (ForceBasedScene, HypergraphEdit) in (mpScene, mpView)
    // We have to get rid of them and replace them by a pair of (CommonConceptGraphScene, CommonConceptGraphEditor)
    HypergraphScene *old = mpScene;
    HypergraphEdit  *old2 = mpView;

    mpCommonConceptScene = new CommonConceptGraphScene();
    mpCommonConceptEditor = new CommonConceptGraphEditor(mpCommonConceptScene);
    mpScene = mpCommonConceptScene;
    mpView = mpCommonConceptEditor;
    mpView->show();
    QLayout *layout = mpUi->View->layout();
    if (layout)
    {
        delete layout;
    }
    layout = new QVBoxLayout();
    layout->addWidget(mpView);
    mpUi->View->setLayout(layout);

    delete old2;
    delete old;

    // Connect
    connect(mpCommonConceptScene, SIGNAL(conceptAdded(const unsigned)), this, SLOT(onGraphChanged(const unsigned)));
    connect(mpCommonConceptScene, SIGNAL(conceptRemoved(const unsigned)), this, SLOT(onGraphChanged(const unsigned)));
    connect(mpCommonConceptScene, SIGNAL(relationAdded(const unsigned)), this, SLOT(onGraphChanged(const unsigned)));
    connect(mpCommonConceptScene, SIGNAL(relationRemoved(const unsigned)), this, SLOT(onGraphChanged(const unsigned)));
}

CommonConceptGraphWidget::~CommonConceptGraphWidget()
{
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

void CommonConceptGraphWidget::onGraphChanged(const unsigned id)
{
    // Gets triggered whenever a concept||relations has been added||removed
    mpUi->statsLabel->setText(
                            "CONCEPTS: " + QString::number(mpConceptScene->graph()->find().size()) +
                            "  RELATIONS: " + QString::number(mpConceptScene->graph()->relations().size())
                             );
}
