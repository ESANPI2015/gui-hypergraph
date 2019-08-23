#include "ConceptgraphViewer.hpp"
#include "ui_HypergraphViewer.h"
#include "ConceptgraphItem.hpp"

#include <QGraphicsScene>
#include <QWheelEvent>
#include <QTimer>
#include <QtCore>
#include <QInputDialog>

#include "Hypergraph.hpp"
#include "Conceptgraph.hpp"
#include "HypergraphYAML.hpp"
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
    if (id.empty())
        currentConceptGraph.concept(label.toStdString(), label.toStdString());
    else
        currentConceptGraph.concept(id, label.toStdString());
    visualize();
}

void ConceptgraphScene::addRelation(const UniqueId fromId, const UniqueId toId, const UniqueId id, const QString& label)
{
    if (id.empty())
        currentConceptGraph.relate(Hyperedges{fromId}, Hyperedges{toId}, label.toStdString());
    else
        currentConceptGraph.relate(id, Hyperedges{fromId}, Hyperedges{toId}, label.toStdString());
    visualize();
}

void ConceptgraphScene::removeEdge(const UniqueId id)
{
    currentConceptGraph.destroy(id);
    visualize();
}

void ConceptgraphScene::updateEdge(const UniqueId id, const QString& label)
{
    currentConceptGraph.access(id).updateLabel(label.toStdString());
    visualize();
}

void ConceptgraphScene::visualize(const Conceptgraph& graph)
{
    // Merge & visualize
    currentConceptGraph.importFrom(graph);
    visualize();
}

void ConceptgraphScene::visualize()
{
    // Suppress visualisation if desired
    if (!isEnabled())
        return;

    // Make a snapshot of the current graph
    Conceptgraph snapshot(this->graph());
    currentGraph = snapshot;

    // Now get all edges of the graph
    auto allConcepts(snapshot.concepts());
    auto allRelations(snapshot.relations());

    // Then we go through all edges and check if we already have an ConceptgraphItem or not
    QMap<UniqueId,ConceptgraphItem*> validItems;
    for (auto relId : allRelations)
    {
        // Skip basic models
        if (relId == Conceptgraph::IsConceptId)
            continue;
        if (relId == Conceptgraph::IsRelationId)
            continue;
        // Create or get item
        ConceptgraphItem *item;
        if (!currentItems.contains(relId))
        {
            item = new ConceptgraphItem(relId, ConceptgraphItem::RELATION);
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
            item = new ConceptgraphItem(conceptId, ConceptgraphItem::CONCEPT);
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
        auto edge = snapshot.access(edgeId);
        // Make sure that item and edge share the same label
        srcItem->setLabel(QString::fromStdString(edge.label()));
        for (auto otherId : edge.pointingTo())
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
        for (auto otherId : edge.pointingFrom())
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
        removeItem(item);
        delete item;
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
        // Ask for URI
        bool ok;
        QString uri = QInputDialog::getText(this, tr("New Concept"), tr("Unqiue Identifier:"), QLineEdit::Normal, tr("domain::type::subtype"), &ok);
        if (ok && !uri.isEmpty())
        {
            scene()->addConcept(uri.toStdString(), currentLabel);
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
            // Ask for URI
            bool ok;
            QString uri = QInputDialog::getText(this, tr("New Relation"), tr("Unqiue Identifier:"), QLineEdit::Normal, tr("domain::type::subtype"), &ok);
            if (ok && !uri.isEmpty())
            {
                // Add model edge
                scene()->addRelation(sourceItem->getHyperEdgeId(), edge->getHyperEdgeId(), uri.toStdString(), currentLabel);
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

    HypergraphView::mouseReleaseEvent(event);
}

ConceptgraphWidget::ConceptgraphWidget(QWidget *parent, bool doSetup)
: HypergraphViewer(parent, false)
{
    if (doSetup)
    {
        // Setup my own ui
        mpUi = new Ui::HypergraphViewer();
        mpUi->setupUi(this);

        mpConceptScene = new ConceptgraphScene();
        mpConceptEditor = new ConceptgraphEditor(mpConceptScene);
        mpScene = mpConceptScene;
        mpView = mpConceptEditor;
        mpView->show();
        QVBoxLayout *layout = new QVBoxLayout();
        layout->addWidget(mpView);
        mpUi->View->setLayout(layout);

        // Connect
        connect(mpConceptScene, SIGNAL(itemAdded(QGraphicsItem*)), this, SLOT(onGraphChanged(QGraphicsItem*)));
        connect(mpConceptScene, SIGNAL(conceptAdded(const UniqueId)), this, SLOT(onGraphChanged(const UniqueId)));
        connect(mpConceptScene, SIGNAL(conceptRemoved(const UniqueId)), this, SLOT(onGraphChanged(const UniqueId)));
        connect(mpConceptScene, SIGNAL(relationAdded(const UniqueId)), this, SLOT(onGraphChanged(const UniqueId)));
        connect(mpConceptScene, SIGNAL(relationRemoved(const UniqueId)), this, SLOT(onGraphChanged(const UniqueId)));
    }
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

void ConceptgraphWidget::loadFromGraph(const Conceptgraph& graph)
{
    mpConceptScene->visualize(graph);
}

void ConceptgraphWidget::loadFromYAMLFile(const QString& fileName)
{
    loadFromGraph(Conceptgraph(YAML::LoadFile(fileName.toStdString()).as<Hypergraph>()));
}

void ConceptgraphWidget::loadFromYAML(const QString& yamlString)
{
    loadFromGraph(Conceptgraph(YAML::Load(yamlString.toStdString()).as<Hypergraph>()));
}

void ConceptgraphWidget::onGraphChanged(QGraphicsItem* item)
{
    HyperedgeItem *hitem(dynamic_cast< HyperedgeItem *>(item));
    if (!hitem)
        return;
    // Get current scene pos of view
    QPointF centerOfView(mpConceptEditor->mapToScene(mpConceptEditor->viewport()->rect().center()));
    QPointF noise(qrand() % 100 - 50, qrand() % 100 - 50);
    item->setPos(centerOfView + noise);
}

void ConceptgraphWidget::onGraphChanged(const UniqueId id)
{
    // Gets triggered whenever a concept||relations has been added||removed
    mpUi->statsLabel->setText(
                            "CONCEPTS: " + QString::number(mpConceptScene->graph().concepts().size()) +
                            "  RELATIONS: " + QString::number(mpConceptScene->graph().relations().size())
                             );
}
