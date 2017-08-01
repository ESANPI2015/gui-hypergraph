#include "HypergraphViewer.hpp"
#include "ui_HypergraphViewer.h"
#include "HyperedgeItem.hpp"

#include <QGraphicsScene>
#include <QWheelEvent>
#include <QTimer>
#include <QtCore>

#include "Hyperedge.hpp"
#include "Hypergraph.hpp"
#include "HyperedgeYAML.hpp"
#include <sstream>
#include <iostream>

HypergraphScene::HypergraphScene(QObject * parent)
: QGraphicsScene(parent)
{
    currentGraph = new Hypergraph();
}

HypergraphScene::~HypergraphScene()
{
    if (currentGraph)
        delete currentGraph;
}

void HypergraphScene::addItem(QGraphicsItem *item)
{
    QGraphicsScene::addItem(item);

    // Emit signals
    HyperedgeItem *edge = dynamic_cast<HyperedgeItem*>(item);
    if (edge)
    {
        emit edgeAdded(edge->getHyperEdgeId());
    }
    EdgeItem *conn = dynamic_cast<EdgeItem*>(item);
    if (conn)
    {
        emit edgesConnected(conn->getSourceItem()->getHyperEdgeId(), conn->getTargetItem()->getHyperEdgeId());
    }
}

void HypergraphScene::removeItem(QGraphicsItem *item)
{
    // Emit signals
    HyperedgeItem *edge = dynamic_cast<HyperedgeItem*>(item);
    if (edge)
    {
        emit edgeRemoved(edge->getHyperEdgeId());
    }

    QGraphicsScene::removeItem(item);
}

void HypergraphScene::addEdge(const QString& label)
{
    if (currentGraph)
    {
        // Find a nice available id for it
        unsigned id = qHash(label);
        while (!currentGraph->create(id, label.toStdString())) id++;
    }
}

void HypergraphScene::addEdgeAndConnect(const unsigned int toId, const QString& label)
{
    // FIXME: This is now difficult. Do we want to add it to the from or the to set?
    if (currentGraph)
    {
        // Find a nice available id for it
        unsigned id = qHash(label);
        while (!currentGraph->create(id, label.toStdString())) id++;
        currentGraph->to(id, toId);
    }
}

void HypergraphScene::removeEdge(const unsigned int id)
{
    if (currentGraph)
        currentGraph->destroy(id);
}

void HypergraphScene::connectEdges(const unsigned int fromId, const unsigned int toId)
{
    // FIXME: This is now difficult. Do we want to add it to the from or the to set?
    if (currentGraph)
        currentGraph->to(fromId, toId);
}

void HypergraphScene::updateEdge(const unsigned int id, const QString& label)
{
    if (currentGraph)
        currentGraph->get(id)->updateLabel(label.toStdString());
}

void HypergraphScene::visualize(Hypergraph* graph)
{
    // If new graph is given, ...
    if (graph)
    {
        // Merge graphs
        auto mergedGraph = new Hypergraph(*currentGraph, *graph);
        // destroy old one
        delete currentGraph;
        currentGraph = mergedGraph;
    }

    // Now get all edges of the graph
    auto allEdges = currentGraph->find();

    // Then we go through all edges and check if we already have an HyperedgeItem or not
    QMap<unsigned int,HyperedgeItem*> validItems;
    for (auto edgeId : allEdges)
    {
        auto x = currentGraph->get(edgeId);
        if (!x)
            continue;
        // Create or get item
        HyperedgeItem *item;
        if (!currentItems.contains(x->id()))
        {
            item = new HyperedgeItem(x);
            item->setPos(qrand() % 2000 - 1000, qrand() % 2000 - 1000);
            addItem(item);
            currentItems[x->id()] = item;
        } else {
            item = currentItems[x->id()];
        }
        validItems[x->id()] = item;
    }
    
    // Everything which is in validItem should be wired
    QMap<unsigned int,HyperedgeItem*>::const_iterator it;
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
                if (line->getType() != EdgeItem::TO)
                    continue;
                if (line->getTargetItem() == destItem)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                auto line = new EdgeItem(srcItem, destItem, EdgeItem::TO);
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
    QMap<unsigned int,HyperedgeItem*> toBeChecked(currentItems);
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

ForceBasedScene::ForceBasedScene(QObject * parent)
: HypergraphScene(parent)
{
    mpTimer = new QTimer(this);
    connect(mpTimer, SIGNAL(timeout()), this, SLOT(visualize()));
    mpTimer->start(1000/25);

    mEquilibriumDistance = 100;
}

ForceBasedScene::~ForceBasedScene()
{
    delete mpTimer;
}

bool ForceBasedScene::isEnabled()
{
    return mpTimer->isActive();
}

void ForceBasedScene::setEnabled(bool enable)
{
    if (enable && !isEnabled())
        mpTimer->start();
    else if (!enable)
        mpTimer->stop();
}

void ForceBasedScene::setEquilibriumDistance(qreal distance)
{
    if (distance > 0)
        mEquilibriumDistance = distance;
}

void ForceBasedScene::visualize(Hypergraph *graph)
{
    // First reconstruct the scene
    HypergraphScene::visualize(graph);

    // This is similar to Graph Drawing by Force-directed  Placement THOMAS M. J. FRUCHTERMAN* AND EDWARD M. REINGOLD 
    qreal k = mEquilibriumDistance;
    qreal k_sqr = k * k;

    // Cycle through pairs of nodes
    QMap<HyperedgeItem*, QPointF> displacements;
    for (auto item : items())
    {
        auto edge = dynamic_cast<HyperedgeItem*>(item);
        if (!edge) continue;
        displacements[edge] = QPointF(0,0);
        for (auto otherItem : items())
        {
            auto other = dynamic_cast<HyperedgeItem*>(otherItem);
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
        
        auto source = line->getSourceItem();
        auto target = line->getTargetItem();
        QPointF delta = source->pos() - target->pos();
        qreal length = qSqrt(delta.x() * delta.x() + delta.y() * delta.y());
        // Calculate attractive forces
        displacements[source] -= length * delta / k; // d^2/k
        displacements[target] += length * delta / k; // d^2/k
    }

    // Update positions
    for (auto item : items())
    {
        auto edge = dynamic_cast<HyperedgeItem*>(item);
        if (!edge) continue;

        edge->setPos(item->pos() + displacements[edge] / mEquilibriumDistance / 10);
    }
}

HypergraphView::HypergraphView(QWidget *parent)
: QGraphicsView(parent)
{
    setAcceptDrops(true);
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing |
                       QPainter::SmoothPixmapTransform);
}

HypergraphView::HypergraphView ( HypergraphScene * scene, QWidget * parent)
: QGraphicsView(scene, parent)
{
    setAcceptDrops(true);
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing |
                       QPainter::SmoothPixmapTransform);
}

void HypergraphView::wheelEvent(QWheelEvent *event)
{
    scaleView(pow(2.0, -event->delta() / 240.0));
}

void HypergraphView::scaleView(qreal scaleFactor)
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

void HypergraphView::mouseReleaseEvent(QMouseEvent* event)
{
    // always try to reset drag mode, just to be sure
    if (dragMode() != QGraphicsView::NoDrag) {
        setDragMode(QGraphicsView::NoDrag);
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void HypergraphView::mousePressEvent(QMouseEvent* event)
{
    QGraphicsItem *item = itemAt(event->pos());
    if (event->button() == Qt::LeftButton)
    {
        // Panning mode
        // enable panning by pressing+dragging the left mouse button if there is
        // _no_ HyperedgeItem under the cursor right now.
        // else we have select item mode (if there is an item)
        if (!item || !dynamic_cast<HyperedgeItem*>(item))
        {
            setDragMode(QGraphicsView::ScrollHandDrag);
        }
    }

    QGraphicsView::mousePressEvent(event);
}

HypergraphEdit::HypergraphEdit(QWidget *parent)
: HypergraphView(parent)
{
    selectedItem = NULL;
    lineItem = NULL;
    isDrawLineMode = false;
    currentLabel = "Name?";
    isEditLabelMode = false;
}

HypergraphEdit::HypergraphEdit(HypergraphScene * scene, QWidget * parent)
: HypergraphView(scene, parent)
{
    selectedItem = NULL;
    lineItem = NULL;
    isDrawLineMode = false;
    currentLabel = "Name?";
    isEditLabelMode = false;
}

void HypergraphEdit::keyPressEvent(QKeyEvent * event)
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
        if (selectedItem)
        {
            // Add an edge to the graph
            scene()->addEdgeAndConnect(selectedItem->getHyperEdgeId(), currentLabel);
        } else {
            scene()->addEdge(currentLabel);
        }
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

void HypergraphEdit::mousePressEvent(QMouseEvent* event)
{
    QGraphicsItem *item = itemAt(event->pos());
    if (event->button() == Qt::LeftButton)
    {
        isEditLabelMode = false;
        if (item)
        {
            // Select only HyperedgeItems
            HyperedgeItem *edge = dynamic_cast<HyperedgeItem*>(item);
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
        // If pressed on a HyperedgeItem we start drawing a line
        auto edge = dynamic_cast<HyperedgeItem*>(item);
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

void HypergraphEdit::mouseMoveEvent(QMouseEvent* event)
{
    // When in DRAW_LINE mode, update temporary visual line
    if (isDrawLineMode)
    {
        lineItem->setLine(sourceItem->pos().x(), sourceItem->pos().y(), mapToScene(event->pos()).x(), mapToScene(event->pos()).y());
    }

    QGraphicsView::mouseMoveEvent(event);
}

void HypergraphEdit::mouseReleaseEvent(QMouseEvent* event)
{
    QGraphicsItem *item = itemAt(event->pos());
    if ((event->button() == Qt::RightButton) && isDrawLineMode)
    {
        // Check if there is a item at current pos
        auto edge = dynamic_cast<HyperedgeItem*>(item);
        if (edge)
        {
            // Add model edge
            scene()->connectEdges(sourceItem->getHyperEdgeId(), edge->getHyperEdgeId());
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

void HypergraphEdit::setDefaultLabel(const QString& label)
{
    currentLabel = label;
    emit labelChanged(currentLabel);
}

HypergraphViewer::HypergraphViewer(QWidget *parent)
    : QWidget(parent)
{
    mpUi = new Ui::HypergraphViewer();
    mpUi->setupUi(this);

    mpScene = new ForceBasedScene();
    mpView = new HypergraphEdit(mpScene);
    mpView->show();
    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(mpView);
    mpUi->View->setLayout(layout);

    mpUi->usageLabel->setText("LMB: Select  RMB: Associate  WHEEL: Zoom  DEL: Delete  INS: Insert");

}

HypergraphViewer::~HypergraphViewer()
{
    delete mpView;
    delete mpScene;
    delete mpUi;
}

void HypergraphViewer::loadFromYAMLFile(const QString& fileName)
{
    auto newGraph = YAML::LoadFile(fileName.toStdString()).as<Hypergraph*>(); // std::string >> YAML::Node >> Hypergraph*
    mpScene->visualize(newGraph);
    delete newGraph;
}

void HypergraphViewer::loadFromYAML(const QString& yamlString)
{
    auto newGraph = YAML::Load(yamlString.toStdString()).as<Hypergraph*>();
    mpScene->visualize(newGraph);
    delete newGraph;
}

void HypergraphViewer::storeToYAML()
{
    if (mpScene->graph())
    {
        std::stringstream result;
        YAML::Node node;
        node = mpScene->graph();
        result << node;
        emit YAMLStringReady(QString::fromStdString(result.str()));
    }
}

void HypergraphViewer::clearHypergraph()
{
    if (mpScene->graph())
    {
        auto edges = mpScene->graph()->find();
        for (auto edgeId : edges)
            mpScene->graph()->destroy(edgeId);
    }
}