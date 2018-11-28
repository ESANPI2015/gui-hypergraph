#include "HypergraphViewer.hpp"
#include "ui_HypergraphViewer.h"
#include "HyperedgeItem.hpp"

#include <QGraphicsScene>
#include <QWheelEvent>
#include <QTimer>
#include <QtCore>
#include <QInputDialog>

#include "Hyperedge.hpp"
#include "Hypergraph.hpp"
#include "HypergraphYAML.hpp"
#include <sstream>
#include <iostream>

HypergraphScene::HypergraphScene(QObject * parent)
: QGraphicsScene(parent)
{
}

HypergraphScene::~HypergraphScene()
{
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

    // This signal can be used to let the viewport center on that item or to move it to the center of the view
    emit itemAdded(item);
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

void HypergraphScene::addEdge(const UniqueId id, const QString& label)
{
    currentGraph.create(id, label.toStdString());
    visualize();
}

void HypergraphScene::removeEdge(const UniqueId id)
{
    currentGraph.destroy(id);
    visualize();
}

void HypergraphScene::connectEdges(const UniqueId fromId, const UniqueId id, const UniqueId toId)
{
    if (!fromId.empty())
        currentGraph.from(Hyperedges{fromId}, Hyperedges{id});
    if (!toId.empty())
        currentGraph.to(Hyperedges{id}, Hyperedges{toId});
    visualize();
}

void HypergraphScene::updateEdge(const UniqueId id, const QString& label)
{
    currentGraph.get(id).updateLabel(label.toStdString());
    visualize();
}

void HypergraphScene::visualize(const Hypergraph& graph)
{
    // Merge
    currentGraph.importFrom(graph);

    // ... and visualize
    visualize();
}

void HypergraphScene::visualize()
{
    // Suppress visualisation if desired
    if (!isEnabled())
        return;

    // Now get all edges of the graph
    auto allEdges = currentGraph.find();

    // Then we go through all edges and check if we already have an HyperedgeItem or not
    QMap<UniqueId,HyperedgeItem*> validItems;
    for (auto edgeId : allEdges)
    {
        if (!currentGraph.exists(edgeId))
            continue;
        // Create or get item
        HyperedgeItem *item;
        if (!currentItems.contains(edgeId))
        {
            item = new HyperedgeItem(edgeId);
            addItem(item);
            currentItems[edgeId] = item;
        } else {
            item = currentItems[edgeId];
        }
        validItems[edgeId] = item;
    }
    
    // Everything which is in validItem should be wired
    QMap<UniqueId,HyperedgeItem*>::const_iterator it;
    for (it = validItems.begin(); it != validItems.end(); ++it)
    {
        auto edgeId = it.key();
        auto srcItem = it.value();
        auto edge = currentGraph.read(edgeId);
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
        for (auto otherId : edge.pointingFrom())
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
    QMap<UniqueId,HyperedgeItem*> toBeChecked(currentItems);
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
    QList<QGraphicsItem*> allItems(items());
    QList<QGraphicsItem*> excludedItems(selectedItems());

    // Remove all selected items from allItems
    for (auto item : excludedItems)
    {
        allItems.removeAll(item);
    }

    // Remove invisible items from allItems
    excludedItems = allItems;
    for (auto item : excludedItems)
    {
        if (!item->isVisible())
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
    //qreal factor = transform()
    transform()
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
        // Ask for URI
        bool ok;
        QString uri = QInputDialog::getText(this, tr("New Hyperedge"), tr("Unqiue Identifier:"), QLineEdit::Normal, tr("domain::type::subtype"), &ok);
        if (ok && !uri.isEmpty())
        {
            scene()->addEdge(uri.toStdString(), currentLabel);
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
        lineItem->setLine(sourceItem->scenePos().x(), sourceItem->scenePos().y(), mapToScene(event->pos()).x(), mapToScene(event->pos()).y());
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
            // Ask for Direction
            bool ok;
            QStringList items;
            items << tr("Points To") << tr("Points From");
            QString direction = QInputDialog::getItem(this, tr("New Link"), tr("Direction:"), items, 0, false, &ok);
            if (ok && !direction.isEmpty())
            {
                // Add model edge
                if (direction == tr("Points To")) 
                    scene()->connectEdges("", sourceItem->getHyperEdgeId(), edge->getHyperEdgeId());
                else
                    scene()->connectEdges(edge->getHyperEdgeId(), sourceItem->getHyperEdgeId(), "");
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

void HypergraphEdit::setDefaultLabel(const QString& label)
{
    currentLabel = label;
    emit labelChanged(currentLabel);
}

HypergraphViewer::HypergraphViewer(QWidget *parent, bool doSetup)
    : QWidget(parent)
{
    if (doSetup)
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

        connect(mpScene, SIGNAL(itemAdded(QGraphicsItem*)), this, SLOT(onGraphChanged(QGraphicsItem*)));
    } else {
        mpUi = NULL;
        mpScene = NULL;
        mpView = NULL;
    }
}

HypergraphViewer::~HypergraphViewer()
{
    if (mpView)
        delete mpView;
    if (mpScene)
        delete mpScene;
    if (mpUi)
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

void HypergraphViewer::loadFromGraph(const Hypergraph& graph)
{
    mpScene->visualize(graph);
}

void HypergraphViewer::loadFromYAMLFile(const QString& fileName)
{
    loadFromGraph(YAML::LoadFile(fileName.toStdString()).as<Hypergraph>());
}

void HypergraphViewer::loadFromYAML(const QString& yamlString)
{
    loadFromGraph(YAML::Load(yamlString.toStdString()).as<Hypergraph>());
}

void HypergraphViewer::storeToYAML()
{
    emit YAMLStringReady(QString::fromStdString(YAML::StringFrom(mpScene->graph())));
}

void HypergraphViewer::onGraphChanged(const UniqueId id)
{
    // update stats
    mpUi->statsLabel->setText("HE: " + QString::number(mpScene->graph().find().size()));
}

void HypergraphViewer::onGraphChanged(QGraphicsItem* item)
{
    HyperedgeItem *hitem(dynamic_cast< HyperedgeItem *>(item));
    if (!hitem)
        return;
    // Get current scene pos of view
    QPointF centerOfView(mpView->mapToScene(mpView->viewport()->rect().center()));
    QPointF noise(qrand() % 100 - 50, qrand() % 100 - 50);
    item->setPos(centerOfView + noise);
}

void HypergraphViewer::clearHypergraph()
{
    auto edges = mpScene->graph().find();
    for (auto edgeId : edges)
        mpScene->graph().destroy(edgeId);
    // update stats
    mpUi->statsLabel->setText("HE: " + QString::number(mpScene->graph().find().size()));
}

void HypergraphViewer::setEquilibriumDistance(qreal distance)
{
    mpScene->setEquilibriumDistance(distance);
}
