#include "ConceptgraphViewer.hpp"
#include "ui_HypergraphViewer.h"
#include "ConceptgraphItem.hpp"

#include <QGraphicsScene>
#include <QWheelEvent>
#include <QTimer>
#include <QtCore>

#include "Hypergraph.hpp"
#include "Conceptgraph.hpp"
#include "HyperedgeYAML.hpp"
#include <sstream>
#include <iostream>

ConceptgraphScene::ConceptgraphScene(QObject * parent)
: ForceBasedScene(parent)
{
}

ConceptgraphScene::~ConceptgraphScene()
{
}

QList<ConceptgraphItem*> ConceptgraphScene::selectedConceptgraphItems()
{
    // get all selected hyperedge items
    QList<QGraphicsItem *> selItems = selectedItems();
    QList<ConceptgraphItem *> selHItems;
    for (QGraphicsItem *item : selItems)
    {
        ConceptgraphItem* hitem = dynamic_cast<ConceptgraphItem*>(item);
        if (hitem)
            selHItems.append(hitem);
    }
    return selHItems;
}

void ConceptgraphScene::addItem(QGraphicsItem *item)
{
    ForceBasedScene::addItem(item);

    // Emit signals
    ConceptgraphItem *edge = dynamic_cast<ConceptgraphItem*>(item);
    if (edge)
    {
        if (edge->getType() == ConceptgraphItem::CONCEPT)
            emit conceptAdded(edge->getHyperEdgeId());
        else
            emit relationAdded(edge->getHyperEdgeId());
    }
}

void ConceptgraphScene::removeItem(QGraphicsItem *item)
{
    // Emit signals
    ConceptgraphItem *edge = dynamic_cast<ConceptgraphItem*>(item);
    if (edge)
    {
        if (edge->getType() == ConceptgraphItem::CONCEPT)
            emit conceptRemoved(edge->getHyperEdgeId());
        else
            emit relationRemoved(edge->getHyperEdgeId());
    }

    ForceBasedScene::removeItem(item);
}

void ConceptgraphScene::addConcept(const UniqueId id, const QString& label)
{
    Conceptgraph *g = graph();
    if (g)
    {
        if (id.empty())
            g->create(label.toStdString());
        else
            g->create(id, label.toStdString());
        visualize();
    }
}

void ConceptgraphScene::addRelation(const UniqueId fromId, const UniqueId toId, const UniqueId id, const QString& label)
{
    Conceptgraph *g = graph();
    if (g)
    {
        if (id.empty())
            g->relate(Hyperedges{toId}, Hyperedges{fromId}, label.toStdString());
        else
            g->relate(id, Hyperedges{toId}, Hyperedges{fromId}, label.toStdString());
        visualize();
    }
}

void ConceptgraphScene::removeEdge(const UniqueId id)
{
    Conceptgraph *g = graph();
    if (g)
    {
        g->destroy(id);
        visualize();
    }
}

void ConceptgraphScene::updateEdge(const UniqueId id, const QString& label)
{
    Conceptgraph *g = graph();
    if (g)
    {
        g->get(id)->updateLabel(label.toStdString());
        visualize();
    }
}

void ConceptgraphScene::visualize(Conceptgraph* graph)
{
    // If a new graph is given, ...
    if (graph)
    {
        if (currentGraph)
        {
            // Merge graphs
            Hypergraph merged = Hypergraph(*currentGraph, *graph);
            Conceptgraph* mergedGraph = new Conceptgraph(merged);
            // destroy old one
            delete currentGraph;
            currentGraph = mergedGraph;
        } else {
            currentGraph = new Conceptgraph(*graph);
        }
    }

    // If we dont have any graph ... skip
    if (!this->graph())
        return;

    // Suppress visualisation if desired
    if (!isEnabled())
        return;

    // Now get all edges of the graph
    auto allConcepts = this->graph()->find();
    auto allRelations = this->graph()->relations();
    int N = allConcepts.size() + allRelations.size();
    int dim = N * mEquilibriumDistance / 2;

    // Then we go through all edges and check if we already have an ConceptgraphItem or not
    QMap<UniqueId,ConceptgraphItem*> validItems;
    for (auto relId : allRelations)
    {
        // Create or get item
        ConceptgraphItem *item;
        if (!currentItems.contains(relId))
        {
            item = new ConceptgraphItem(this->graph()->get(relId), ConceptgraphItem::RELATION);
            item->setPos(qrand() % dim - dim/2, qrand() % dim - dim/2);
            addItem(item);
            currentItems[relId] = item;
        } else {
            item = dynamic_cast<ConceptgraphItem*>(currentItems[relId]);
        }
        validItems[relId] = item;
    }
    for (auto conceptId : allConcepts)
    {
        // Create or get item
        ConceptgraphItem *item;
        if (!currentItems.contains(conceptId))
        {
            item = new ConceptgraphItem(this->graph()->get(conceptId), ConceptgraphItem::CONCEPT);
            item->setPos(qrand() % dim - dim/2, qrand() % dim - dim/2);
            addItem(item);
            currentItems[conceptId] = item;
        } else {
            item = dynamic_cast<ConceptgraphItem*>(currentItems[conceptId]);
        }
        validItems[conceptId] = item;
    }

    // Everything which is in validItem should be wired
    QMap<UniqueId,ConceptgraphItem*>::const_iterator it;
    for (it = validItems.begin(); it != validItems.end(); ++it)
    {
        auto edgeId = it.key();
        auto srcItem = it.value();
        auto edge = this->graph()->get(edgeId);
        // Make sure that item and edge share the same label
        srcItem->setLabel(QString::fromStdString(edge->label()));
        for (auto otherId : edge->pointingTo())
        {
            if (!validItems.contains(otherId))
                continue;
            // Create line if needed
            auto destItem = validItems[otherId];
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
        for (auto otherId : edge->pointingFrom())
        {
            if (!validItems.contains(otherId))
                continue;
            // Create line if needed
            auto destItem = validItems[otherId];
            // Omit loops
            if (srcItem == destItem)
                continue;
            // Check if there is an edgeitem of type FROM which points to destItem
            bool found = false;
            auto myEdgeItems = srcItem->getEdgeItems();
            for (auto line : myEdgeItems)
            {
                if (line->getType() != EdgeItem::FROM)
                    continue;
                if (line->getTargetItem() == destItem)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                auto line = new EdgeItem(srcItem, destItem, EdgeItem::FROM);
                addItem(line);
            }
        }
    }

    // Everything which is in currentItems but not in validItems has to be removed
    // First: remove edges
    QMap<UniqueId,HyperedgeItem*> toBeChecked(currentItems); // NOTE: Since we modify currentItems, we should make a snapshot of the current state
    QMap<UniqueId,HyperedgeItem*>::const_iterator it2;
    for (it2 = toBeChecked.begin(); it2 != toBeChecked.end(); ++it2)
    {
        auto id = it2.key();
        if (validItems.contains(id))
            continue;
        auto item = it2.value();
        auto edgeSet = item->getEdgeItems();
        for (auto edge : edgeSet)
        {
            edge->deregister();
            delete edge;
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
        currentItems.remove(id);
        //delete item;
        removeItem(item);
        item->deleteLater();
    }
}

ConceptgraphEditor::ConceptgraphEditor(QWidget *parent)
: HypergraphEdit(parent)
{
}

ConceptgraphEditor::ConceptgraphEditor(ConceptgraphScene * scene, QWidget * parent)
: HypergraphEdit(scene, parent)
{
}

void ConceptgraphEditor::keyPressEvent(QKeyEvent * event)
{
    QList<ConceptgraphItem *> selection = scene()->selectedConceptgraphItems();
    if (event->key() == Qt::Key_Delete)
    {
        for (ConceptgraphItem *item : selection)
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
        for (ConceptgraphItem *item : selection)
        {
            scene()->updateEdge(item->getHyperEdgeId(), currentLabel);
        }
        setDefaultLabel(currentLabel);
    }

    QGraphicsView::keyPressEvent(event);
}

void ConceptgraphEditor::mousePressEvent(QMouseEvent* event)
{
    QGraphicsItem *item = itemAt(event->pos());
    if (event->button() == Qt::LeftButton)
    {
        isEditLabelMode = false;
    }
    else if (event->button() == Qt::RightButton)
    {
        // If pressed on a ConceptgraphItem we start drawing a line
        auto edge = dynamic_cast<ConceptgraphItem*>(item);
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

    HypergraphView::mousePressEvent(event);
}

void ConceptgraphEditor::mouseReleaseEvent(QMouseEvent* event)
{
    QGraphicsItem *item = itemAt(event->pos());
    if ((event->button() == Qt::RightButton) && isDrawLineMode)
    {
        // Check if there is a ConceptgraphItem at current pos
        auto edge = dynamic_cast<ConceptgraphItem*>(item);
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

    HypergraphView::mouseReleaseEvent(event);
}

ConceptgraphWidget::ConceptgraphWidget(QWidget *parent)
: HypergraphViewer(parent)
{
    // The base class constructor created a pair of (ForceBasedScene, HypergraphEdit) in (mpScene, mpView)
    // We have to get rid of them and replace them by a pair of (ConceptgraphScene, ConceptgraphEditor)
    HypergraphScene *old = mpScene;
    HypergraphEdit  *old2 = mpView;

    mpConceptScene = new ConceptgraphScene();
    mpConceptEditor = new ConceptgraphEditor(mpConceptScene);
    mpScene = mpConceptScene;
    mpView = mpConceptEditor;
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
    connect(mpConceptScene, SIGNAL(conceptAdded(const UniqueId)), this, SLOT(onGraphChanged(const UniqueId)));
    connect(mpConceptScene, SIGNAL(conceptRemoved(const UniqueId)), this, SLOT(onGraphChanged(const UniqueId)));
    connect(mpConceptScene, SIGNAL(relationAdded(const UniqueId)), this, SLOT(onGraphChanged(const UniqueId)));
    connect(mpConceptScene, SIGNAL(relationRemoved(const UniqueId)), this, SLOT(onGraphChanged(const UniqueId)));
}

ConceptgraphWidget::~ConceptgraphWidget()
{
}

void ConceptgraphWidget::showEvent(QShowEvent *event)
{
    // About to be shown
    mpConceptScene->setEnabled(true);
    mpConceptScene->visualize();
}

void ConceptgraphWidget::loadFromGraph(Conceptgraph& graph)
{
    mpConceptScene->visualize(&graph);
}

void ConceptgraphWidget::loadFromYAMLFile(const QString& fileName)
{
    Hypergraph* newGraph = YAML::LoadFile(fileName.toStdString()).as<Hypergraph*>(); // std::string >> YAML::Node >> Hypergraph* >> Conceptgraph
    Conceptgraph* newCGraph = new Conceptgraph(*newGraph);
    loadFromGraph(*newCGraph);
    delete newCGraph;
    delete newGraph;
}

void ConceptgraphWidget::loadFromYAML(const QString& yamlString)
{
    auto newGraph = YAML::Load(yamlString.toStdString()).as<Hypergraph*>();
    Conceptgraph* newCGraph = new Conceptgraph(*newGraph);
    loadFromGraph(*newCGraph);
    delete newCGraph;
    delete newGraph;
}

void ConceptgraphWidget::onGraphChanged(const UniqueId id)
{
    // Gets triggered whenever a concept||relations has been added||removed
    mpUi->statsLabel->setText(
                            "CONCEPTS: " + QString::number(mpConceptScene->graph()->find().size()) +
                            "  RELATIONS: " + QString::number(mpConceptScene->graph()->relations().size())
                             );
}
