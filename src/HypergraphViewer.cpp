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
    currentGraph = NULL;
}

HypergraphScene::~HypergraphScene()
{
    if (currentGraph)
    {
        delete currentGraph;
        currentGraph = NULL;
    }
}

QList<HyperedgeItem*> HypergraphScene::selectedHyperedgeItems()
{
    // get all selected hyperedge items
    QList<QGraphicsItem *> selItems = selectedItems();
    QList<HyperedgeItem *> selHItems;
    for (QGraphicsItem *item : selItems)
    {
        HyperedgeItem* hitem = dynamic_cast<HyperedgeItem*>(item);
        if (hitem)
            selHItems.append(hitem);
    }
    return selHItems;
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
        while (currentGraph->create(id, label.toStdString()).empty()) id++;
        visualize();
    }
}

void HypergraphScene::removeEdge(const unsigned int id)
{
    if (currentGraph)
    {
        currentGraph->destroy(id);
        visualize();
    }
}

void HypergraphScene::connectEdges(const unsigned int fromId, const unsigned int id, const unsigned int toId)
{
    if (currentGraph)
    {
        currentGraph->from(fromId, id);
        currentGraph->to(id, toId);
        visualize();
    }
}

void HypergraphScene::updateEdge(const unsigned int id, const QString& label)
{
    if (currentGraph)
    {
        currentGraph->get(id)->updateLabel(label.toStdString());
        visualize();
    }
}

void HypergraphScene::visualize(Hypergraph* graph)
{
    // If new graph is given, ...
    if (graph)
    {
        if (currentGraph)
        {
            // Merge graphs
            auto mergedGraph = new Hypergraph(*currentGraph, *graph);
            // destroy old one
            delete currentGraph;
            currentGraph = mergedGraph;
        } else {
            currentGraph = new Hypergraph(*graph);
        }
    }

    // If we dont have any graph ... skip
    if (!currentGraph)
        return;

    // Suppress visualisation if desired
    if (!isEnabled())
        return;

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
    mEquilibriumDistance = 100;
    mpTimer = new QTimer(this);
    connect(mpTimer, SIGNAL(timeout()), this, SLOT(updateLayout()));
    mpTimer->start(1000/25);
    setLayoutEnabled(false);
}

ForceBasedScene::~ForceBasedScene()
{
    mpTimer->stop();
    delete mpTimer;
}

bool ForceBasedScene::isLayoutEnabled()
{
    return mpTimer->isActive();
}

void ForceBasedScene::setLayoutEnabled(bool enable)
{
    if (enable && !isLayoutEnabled())
        mpTimer->start();
    else if (!enable)
        mpTimer->stop();
}

void ForceBasedScene::setEnabled(bool enable)
{
    //setLayoutEnabled(enable);
    HypergraphScene::setEnabled(enable);
}

void ForceBasedScene::setEquilibriumDistance(qreal distance)
{
    if (distance > 0)
        mEquilibriumDistance = distance;
}

void ForceBasedScene::updateLayout()
{
    // Suppress visualisation if desired
    if (!isEnabled())
        return;

    // This is similar to Graph Drawing by Force-directed  Placement THOMAS M. J. FRUCHTERMAN* AND EDWARD M. REINGOLD 
    qreal mEquilibriumDistance_sqr = mEquilibriumDistance * mEquilibriumDistance;
    QMap<HyperedgeItem*, QPointF> displacements;
    QList<QGraphicsItem*> allItems = items();
    QList<QGraphicsItem*> excludedItems = selectedItems();

    // Remove all selected items from allItems
    for (auto item : excludedItems)
    {
        allItems.removeAll(item);
    }

    // Zeroing displacements & filter hyperedgeitems
    QList<HyperedgeItem*> allHyperedgeItems;
    QList<EdgeItem*> allEdgeItems;
    for (auto item : allItems)
    {
        auto edge = dynamic_cast<HyperedgeItem*>(item);
        if (!edge) 
        {
            auto line = dynamic_cast<EdgeItem*>(item);
            if (line)
                allEdgeItems.append(line);
            continue;
        }
        // Ignore children
        if (edge->parentItem())
            continue;
        allHyperedgeItems.append(edge);
        displacements[edge] = QPointF(0.,0.);
    }
    const unsigned int N = allHyperedgeItems.size();

    // Global update
    for (unsigned int i = 0; i < N; ++i)
    {
        HyperedgeItem* selectedEdge = allHyperedgeItems.at(i);
        // For selected edge:
        // a) calculate all repelling forces to other nodes
        for (unsigned int j = i; j < N; ++j)
        {
            HyperedgeItem* other = allHyperedgeItems.at(j);
            if (other == selectedEdge)
                continue;
            QPointF delta(selectedEdge->scenePos() - other->scenePos()); // points towards selectedEdge
            qreal length_sqr = delta.x() * delta.x() + delta.y() * delta.y();

            // Push away according to charge model
            // Actually, charges should be proportional to size
            if (length_sqr > 1e-9)
            {
                qreal length = qSqrt(length_sqr);
                // Skip if distance is one order of magnitude greater than equilibrium distance
                if (length < mEquilibriumDistance * 10.)
                {
                    displacements[selectedEdge] +=  mEquilibriumDistance_sqr * mEquilibriumDistance * delta / length / length_sqr / N; // ~ 1/d^2 * 1/N
                    displacements[other]        -=  mEquilibriumDistance_sqr * mEquilibriumDistance * delta / length / length_sqr / N; // ~ 1/d^2 * 1/N
                }
            } 
        }
    }

    // b) find all EdgeItems and calc attraction forces
    for (auto line : allEdgeItems)
    {
        // Calculate attraction between connected nodes
        auto source = line->getSourceItem();
        auto target = line->getTargetItem();

        // If source || target have parent items use them instead
        if (source->parentItem())
            source = dynamic_cast<HyperedgeItem*>(source->parentItem());
        if (target->parentItem())
            target = dynamic_cast<HyperedgeItem*>(target->parentItem());

        QPointF delta(source->scenePos() - target->scenePos()); // points towards source
        qreal length_sqr = delta.x() * delta.x() + delta.y() * delta.y();

        // Pull according to a spring
        if (length_sqr > 1e-9)
        {
            qreal length = qSqrt(length_sqr);
            displacements[source] -=  (1. - mEquilibriumDistance / length) * delta / N; // ~ d/N
            displacements[target] +=  (1. - mEquilibriumDistance / length) * delta / N;
        } 
    }

    // Update positions
    for (auto edge : allHyperedgeItems)
    {
        // Check for bad values
        if (std::isnan(displacements[edge].x()) || std::isnan(displacements[edge].y()))
            continue;
        if (std::isinf(displacements[edge].x()) || std::isinf(displacements[edge].y()))
            continue;
        QPointF newPos(edge->scenePos() + displacements[edge]); // x = x + disp
        if (edge->parentItem())
        {
            edge->setPos(edge->mapFromScene(newPos));
        } else {
            edge->setPos(newPos);
        }
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
    //if(factor < 0.07 || factor > 100)
    //{
    //    return;
    //}
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
    lineItem = NULL;
    isDrawLineMode = false;
    currentLabel = "Name?";
    isEditLabelMode = false;
}

HypergraphEdit::HypergraphEdit(HypergraphScene * scene, QWidget * parent)
: HypergraphView(scene, parent)
{
    lineItem = NULL;
    isDrawLineMode = false;
    currentLabel = "Name?";
    isEditLabelMode = false;
}

void HypergraphEdit::keyPressEvent(QKeyEvent * event)
{
    QList<HyperedgeItem *> selHItems = scene()->selectedHyperedgeItems();
    if (event->key() == Qt::Key_Delete)
    {
        for (HyperedgeItem *hitem : selHItems)
        {
            // Delete edge from graph
            scene()->removeEdge(hitem->getHyperEdgeId());
        }
    }
    else if (event->key() == Qt::Key_Insert)
    {
        scene()->addEdge(currentLabel);
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
    else if (selHItems.size() && (event->key() != Qt::Key_Control))
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
        for (HyperedgeItem *hitem : selHItems)
        {
            scene()->updateEdge(hitem->getHyperEdgeId(), currentLabel);
        }
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
            //scene()->connectEdges(sourceItem->getHyperEdgeId(), edge->getHyperEdgeId());
            // TODO: Connect via DIALOG or buttons
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

    mpUi->usageLabel->setText("LMB: Select  RMB: Associate  WHEEL: Zoom  DEL: Delete  INS: Insert  PAUSE: Toggle Layouting");
}

HypergraphViewer::~HypergraphViewer()
{
    delete mpView;
    delete mpScene;
    delete mpUi;
}

void HypergraphViewer::showEvent(QShowEvent *event)
{
    // About to be shown
    mpScene->setEnabled(true);
    mpScene->visualize();
}

void HypergraphViewer::hideEvent(QHideEvent *event)
{
    // About to be hidden
    mpScene->setEnabled(false);
}

void HypergraphViewer::loadFromGraph(Hypergraph& graph)
{
    mpScene->visualize(&graph);
    // update stats
    mpUi->statsLabel->setText("HE: " + QString::number(mpScene->graph()->find().size()));
}

void HypergraphViewer::loadFromYAMLFile(const QString& fileName)
{
    auto newGraph = YAML::LoadFile(fileName.toStdString()).as<Hypergraph*>(); // std::string >> YAML::Node >> Hypergraph*
    loadFromGraph(*newGraph);
    delete newGraph;
}

void HypergraphViewer::loadFromYAML(const QString& yamlString)
{
    auto newGraph = YAML::Load(yamlString.toStdString()).as<Hypergraph*>();
    loadFromGraph(*newGraph);
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
    // update stats
    mpUi->statsLabel->setText("HE: " + QString::number(mpScene->graph()->find().size()));
}

void HypergraphViewer::setEquilibriumDistance(qreal distance)
{
    mpScene->setEquilibriumDistance(distance);
}
