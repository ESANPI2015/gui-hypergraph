#include "ConceptgraphViewer.hpp"
#include "ui_HyperedgeViewer.h"
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
: QGraphicsScene(parent)
{
    currentGraph = new Conceptgraph();
}

ConceptgraphScene::~ConceptgraphScene()
{
    if (currentGraph)
        delete currentGraph;
}

void ConceptgraphScene::addItem(QGraphicsItem *item)
{
    QGraphicsScene::addItem(item);

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

    QGraphicsScene::removeItem(item);
}

void ConceptgraphScene::addConcept(const unsigned id, const QString& label)
{
    if (currentGraph)
    {
        if (!id)
            currentGraph->create(label.toStdString());
        else
            currentGraph->create(id, label.toStdString());
    }
}

void ConceptgraphScene::addRelation(const unsigned fromId, const unsigned toId, const unsigned id, const QString& label)
{
    if (currentGraph)
    {
        if (!id)
            currentGraph->relate(fromId, toId, label.toStdString());
        else
            currentGraph->relate(id, fromId, toId, label.toStdString());
    }
}

void ConceptgraphScene::removeEdge(const unsigned int id)
{
    if (currentGraph)
        currentGraph->destroy(id);
}

void ConceptgraphScene::updateEdge(const unsigned int id, const QString& label)
{
    if (currentGraph)
        currentGraph->get(id)->updateLabel(label.toStdString());
}

void ConceptgraphScene::visualize(Conceptgraph* graph)
{
    // If new graph is given, ...
    if (graph)
    {
        // Merge graphs
        Hypergraph merged = Hypergraph(*currentGraph, *graph);
        Conceptgraph* mergedGraph = new Conceptgraph(merged);
        // destroy old one
        delete currentGraph;
        currentGraph = mergedGraph;
    }

    // Now get all edges of the graph
    auto allEdges = currentGraph->Hypergraph::find();
    auto allConcepts = currentGraph->find();

    // Then we go through all edges and check if we already have an ConceptgraphItem or not
    QMap<unsigned int,ConceptgraphItem*> validItems;
    for (auto edgeId : allEdges)
    {
        auto x = currentGraph->get(edgeId);
        if (!x)
            continue;
        // Skip id 1 and 2
        if (x->id() == 1)
            continue;
        if (x->id() == 2)
            continue;
        // Create or get item
        ConceptgraphItem *item;
        if (!currentItems.contains(x->id()))
        {
            item = new ConceptgraphItem(x, allConcepts.count(x->id()) > 0 ? ConceptgraphItem::CONCEPT : ConceptgraphItem::RELATION);
            item->setPos(qrand() % 2000 - 1000, qrand() % 2000 - 1000);
            addItem(item);
            currentItems[x->id()] = item;
        } else {
            item = currentItems[x->id()];
        }
        validItems[x->id()] = item;
    }
    
    // Everything which is in validItem should be wired
    QMap<unsigned int,ConceptgraphItem*>::const_iterator it;
    for (it = validItems.begin(); it != validItems.end(); ++it)
    {
        auto edgeId = it.key();
        auto srcItem = it.value();
        auto edge = currentGraph->get(edgeId);
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
            // Check if there is an edgeitem of type TO which points to destItem
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
    QMap<unsigned int,ConceptgraphItem*> toBeChecked(currentItems);
    for (it = toBeChecked.begin(); it != toBeChecked.end(); ++it)
    {
        auto id = it.key();
        if (validItems.contains(id))
            continue;
        auto item = it.value();
        auto edgeSet = item->getEdgeItems();
        for (auto edge : edgeSet)
        {
            edge->deregister();
            delete edge;
        }
    }

    // Everything which is in currentItems but not in validItems has to be removed
    // Second: remove items
    for (it = toBeChecked.begin(); it != toBeChecked.end(); ++it)
    {
        auto id = it.key();
        if (validItems.contains(id))
            continue;
        auto item = it.value();
        currentItems.remove(id);
        delete item;
    }
}

ForceBasedConceptgraphScene::ForceBasedConceptgraphScene(QObject * parent)
: ConceptgraphScene(parent)
{
    mpTimer = new QTimer(this);
    connect(mpTimer, SIGNAL(timeout()), this, SLOT(visualize()));
    mpTimer->start(1000/25);

    mEquilibriumDistance = 100;
}

ForceBasedConceptgraphScene::~ForceBasedConceptgraphScene()
{
    delete mpTimer;
}

bool ForceBasedConceptgraphScene::isEnabled()
{
    return mpTimer->isActive();
}

void ForceBasedConceptgraphScene::setEnabled(bool enable)
{
    if (enable && !isEnabled())
        mpTimer->start();
    else if (!enable)
        mpTimer->stop();
}

void ForceBasedConceptgraphScene::setEquilibriumDistance(qreal distance)
{
    if (distance > 0)
        mEquilibriumDistance = distance;
}

void ForceBasedConceptgraphScene::visualize(Conceptgraph *graph)
{
    // First reconstruct the scene
    ConceptgraphScene::visualize(graph);

    // This is similar to Graph Drawing by Force-directed  Placement THOMAS M. J. FRUCHTERMAN* AND EDWARD M. REINGOLD 
    qreal k = mEquilibriumDistance;
    qreal k_sqr = k * k;

    // Cycle through pairs of nodes
    QMap<ConceptgraphItem*, QPointF> displacements;
    for (auto item : items())
    {
        auto edge = dynamic_cast<ConceptgraphItem*>(item);
        if (!edge) continue;
        displacements[edge] = QPointF(0,0);
        for (auto otherItem : items())
        {
            auto other = dynamic_cast<ConceptgraphItem*>(otherItem);
            if (!other || (other == edge)) continue;

            // Calculate vector (direction from other to me)
            QPointF delta = edge->pos() - other->pos();
            qreal length_sqr = delta.x() * delta.x() + delta.y() * delta.y() + 1.; // cannot be zero
            // Calculate repulsive forces
            displacements[edge] += k_sqr / length_sqr * delta; // k^2/d
        }
    }

    // Cycle through edges
    for (auto item : items())
    {
        auto line = dynamic_cast<EdgeItem*>(item);
        if (!line) continue;
        
        ConceptgraphItem* source = dynamic_cast<ConceptgraphItem*>(line->getSourceItem());
        ConceptgraphItem* target = dynamic_cast<ConceptgraphItem*>(line->getTargetItem());
        QPointF delta = source->pos() - target->pos();
        qreal length = qSqrt(delta.x() * delta.x() + delta.y() * delta.y());
        // Calculate attractive forces
        displacements[source] -= length * delta / k; // d^2/k
        displacements[target] += length * delta / k; // d^2/k
    }

    // Update positions
    for (auto item : items())
    {
        auto edge = dynamic_cast<ConceptgraphItem*>(item);
        if (!edge) continue;

        edge->setPos(item->pos() + displacements[edge] / mEquilibriumDistance / 10);
    }
}

ConceptgraphView::ConceptgraphView(QWidget *parent)
: QGraphicsView(parent)
{
    setAcceptDrops(true);
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing |
                       QPainter::SmoothPixmapTransform);
}

ConceptgraphView::ConceptgraphView ( ConceptgraphScene * scene, QWidget * parent)
: QGraphicsView(scene, parent)
{
    setAcceptDrops(true);
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing |
                       QPainter::SmoothPixmapTransform);
}

void ConceptgraphView::wheelEvent(QWheelEvent *event)
{
    scaleView(pow(2.0, -event->delta() / 240.0));
}

void ConceptgraphView::scaleView(qreal scaleFactor)
{
    qreal factor = transform()
                       .scale(scaleFactor, scaleFactor)
                       .mapRect(QRectF(0, 0, 1, 1))
                       .width();
    if(factor < 0.07 || factor > 100)
    {
        return;
    }
    scale(scaleFactor, scaleFactor);
}

void ConceptgraphView::mouseReleaseEvent(QMouseEvent* event)
{
    // always try to reset drag mode, just to be sure
    if (dragMode() != QGraphicsView::NoDrag) {
        setDragMode(QGraphicsView::NoDrag);
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void ConceptgraphView::mousePressEvent(QMouseEvent* event)
{
    QGraphicsItem *item = itemAt(event->pos());
    if (event->button() == Qt::LeftButton)
    {
        // Panning mode
        // enable panning by pressing+dragging the left mouse button if there is
        // _no_ ConceptgraphItem under the cursor right now.
        // else we have select item mode (if there is an item)
        if (!item || !dynamic_cast<ConceptgraphItem*>(item))
        {
            setDragMode(QGraphicsView::ScrollHandDrag);
        }
    }

    QGraphicsView::mousePressEvent(event);
}

ConceptgraphEdit::ConceptgraphEdit(QWidget *parent)
: ConceptgraphView(parent)
{
    selectedItem = NULL;
    lineItem = NULL;
    isDrawLineMode = false;
    currentLabel = "Name?";
    isEditLabelMode = false;
}

ConceptgraphEdit::ConceptgraphEdit(ConceptgraphScene * scene, QWidget * parent)
: ConceptgraphView(scene, parent)
{
    selectedItem = NULL;
    lineItem = NULL;
    isDrawLineMode = false;
    currentLabel = "Name?";
    isEditLabelMode = false;
}

void ConceptgraphEdit::keyPressEvent(QKeyEvent * event)
{
    if (event->key() == Qt::Key_Delete)
    {
        if (selectedItem)
        {
            // Delete edge from graph
            scene()->removeEdge(selectedItem->getHyperEdgeId());
            selectedItem = NULL;
        }
    }
    else if (event->key() == Qt::Key_Insert)
    {
        scene()->addConcept(0, currentLabel);
    }
    else if (selectedItem && !isEditLabelMode)
    {
        // Start label edit
        isEditLabelMode = true;
        currentLabel = "";
        currentLabel += event->text();
        scene()->updateEdge(selectedItem->getHyperEdgeId(), currentLabel);
        setDefaultLabel(currentLabel);
    }
    else if (selectedItem && isEditLabelMode)
    {
        // Update current label
        currentLabel += event->text();
        scene()->updateEdge(selectedItem->getHyperEdgeId(), currentLabel);
        setDefaultLabel(currentLabel);
    }

    QGraphicsView::keyPressEvent(event);
}

void ConceptgraphEdit::mousePressEvent(QMouseEvent* event)
{
    QGraphicsItem *item = itemAt(event->pos());
    if (event->button() == Qt::LeftButton)
    {
        isEditLabelMode = false;
        if (item)
        {
            // Select only ConceptgraphItems
            ConceptgraphItem *edge = dynamic_cast<ConceptgraphItem*>(item);
            if (edge)
            {
                if (selectedItem)
                    selectedItem->setHighlight(false);
                selectedItem = edge;
                selectedItem->setHighlight(true);
            } else {
                if (selectedItem)
                    selectedItem->setHighlight(false);
                selectedItem = NULL;
            }
        } else {
            if (selectedItem)
                selectedItem->setHighlight(false);
            selectedItem = NULL;
        }
    }
    else if (event->button() == Qt::RightButton)
    {
        // If pressed on a ConceptgraphItem of type CONCEPT we start drawing a line
        auto edge = dynamic_cast<ConceptgraphItem*>(item);
        if (edge && (edge->getType() == ConceptgraphItem::CONCEPT))
        {
            lineItem = new QGraphicsLineItem();
            QPen pen(Qt::red);
            lineItem->setPen(pen);
            scene()->addItem(lineItem);
            isDrawLineMode = true;
            sourceItem = edge;
        }
    }
    ConceptgraphView::mousePressEvent(event);
}

void ConceptgraphEdit::mouseMoveEvent(QMouseEvent* event)
{
    // When in DRAW_LINE mode, update temporary visual line
    if (isDrawLineMode)
    {
        lineItem->setLine(sourceItem->pos().x(), sourceItem->pos().y(), mapToScene(event->pos()).x(), mapToScene(event->pos()).y());
    }

    QGraphicsView::mouseMoveEvent(event);
}

void ConceptgraphEdit::mouseReleaseEvent(QMouseEvent* event)
{
    QGraphicsItem *item = itemAt(event->pos());
    if ((event->button() == Qt::RightButton) && isDrawLineMode)
    {
        // Check if there is a ConceptgraphItem of type CONCEPT at current pos
        auto edge = dynamic_cast<ConceptgraphItem*>(item);
        if (edge && (edge->getType() == ConceptgraphItem::CONCEPT))
        {
            // Add model edge
            std::cout << "Create relation\n";
            scene()->addRelation(sourceItem->getHyperEdgeId(), edge->getHyperEdgeId(), 0, "NewRelation");
        }
        if (lineItem)
        {
            scene()->removeItem(lineItem);
            delete lineItem;
            lineItem = NULL;
        }
        isDrawLineMode = false;
    }

    ConceptgraphView::mouseReleaseEvent(event);
}

void ConceptgraphEdit::setDefaultLabel(const QString& label)
{
    currentLabel = label;
    emit labelChanged(currentLabel);
}

ConceptgraphViewer::ConceptgraphViewer(QWidget *parent)
    : QWidget(parent)
{
    mpUi = new Ui::HyperedgeViewer();
    mpUi->setupUi(this);

    mpScene = new ForceBasedConceptgraphScene();
    mpView = new ConceptgraphEdit(mpScene);
    mpView->show();
    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(mpView);
    mpUi->View->setLayout(layout);

    mpUi->usageLabel->setText("LMB: Select  RMB: Associate  WHEEL: Zoom  DEL: Delete  INS: Insert");

}

ConceptgraphViewer::~ConceptgraphViewer()
{
    delete mpView;
    delete mpScene;
    delete mpUi;
}

void ConceptgraphViewer::loadFromYAMLFile(const QString& fileName)
{
    Hypergraph* newGraph = YAML::LoadFile(fileName.toStdString()).as<Hypergraph*>(); // std::string >> YAML::Node >> Hypergraph* >> Conceptgraph
    Conceptgraph* newCGraph = new Conceptgraph(*newGraph);
    mpScene->visualize(newCGraph);
    delete newCGraph;
    delete newGraph;
}

void ConceptgraphViewer::loadFromYAML(const QString& yamlString)
{
    auto newGraph = YAML::Load(yamlString.toStdString()).as<Hypergraph*>();
    Conceptgraph* newCGraph = new Conceptgraph(*newGraph);
    mpScene->visualize(newCGraph);
    delete newCGraph;
    delete newGraph;
}

void ConceptgraphViewer::storeToYAML()
{
    if (mpScene->graph())
    {
        std::stringstream result;
        YAML::Node node;
        node = dynamic_cast<Hypergraph*>(mpScene->graph());
        result << node;
        emit YAMLStringReady(QString::fromStdString(result.str()));
    }
}

void ConceptgraphViewer::clearConceptgraphSystem()
{
    if (mpScene->graph())
    {
        auto edges = mpScene->graph()->find();
        for (auto edgeId : edges)
            mpScene->graph()->destroy(edgeId);
    }
}

