#include "HyperedgeViewer.hpp"
#include "ui_HyperedgeViewer.h"
#include "HyperedgeItem.hpp"

#include <QGraphicsScene>
#include <QWheelEvent>
#include <QTimer>
#include <QtCore>

#include "Hyperedge.hpp"
#include "HyperedgeYAML.hpp"
#include <sstream>
#include <iostream>

HyperedgeScene::HyperedgeScene(QObject * parent)
: QGraphicsScene(parent)
{
}

HyperedgeScene::~HyperedgeScene()
{
}

void HyperedgeScene::addItem(QGraphicsItem *item)
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

void HyperedgeScene::removeItem(QGraphicsItem *item)
{
    // Emit signals
    HyperedgeItem *edge = dynamic_cast<HyperedgeItem*>(item);
    if (edge)
    {
        emit edgeRemoved(edge->getHyperEdgeId());
    }

    QGraphicsScene::removeItem(item);
}

void HyperedgeScene::addEdge(const QString& label)
{
    allEdges.insert(Hyperedge::create(label.toStdString())->id());
}

void HyperedgeScene::addEdgeAndConnect(const unsigned int toId, const QString& label)
{
    Hyperedge* target = Hyperedge::find(toId);
    if (target)
    {
        Hyperedge* source = Hyperedge::create(label.toStdString());
        source->pointTo(toId);
        allEdges.insert(source->id());
    }
}

void HyperedgeScene::removeEdge(const unsigned int id)
{
    Hyperedge* edge = Hyperedge::find(id);
    // Already deleted?
    if (!edge) return;
    // Delete the model edge
    delete edge;
}

void HyperedgeScene::connectEdges(const unsigned int fromId, const unsigned int toId)
{
    Hyperedge* source = Hyperedge::find(fromId);
    Hyperedge* target = Hyperedge::find(toId);
    if (source && target)
    {
        source->pointTo(toId);
    }
}

void HyperedgeScene::visualize(Hyperedge::Hyperedges edges)
{
    // First we must merge allEdges & edges
    // Then we go through all edges and check if we already have an HyperedgeItem or not
    allEdges.insert(edges.begin(), edges.end());
    QMap<unsigned int,HyperedgeItem*> validItems;
    for (auto edgeId : allEdges)
    {
        auto x = Hyperedge::find(edgeId);
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
        auto edge = Hyperedge::find(edgeId);
        for (auto otherId : edge->pointingTo())
        {
            if (!validItems.contains(otherId))
                continue;
            // Create line if needed
            auto destItem = validItems[otherId];
            // Omit loops
            if (srcItem == destItem)
                continue;
            if (!srcItem->getEdgeItems().contains(otherId))
            {
                auto line = new EdgeItem(srcItem, destItem);
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
        auto edgeMap = item->getEdgeItems();
        for (auto edge : edgeMap)
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
: HyperedgeScene(parent)
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

void ForceBasedScene::visualize(Hyperedge::Hyperedges edges)
{
    // First reconstruct the scene
    HyperedgeScene::visualize(edges);

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

HyperedgeView::HyperedgeView(QWidget *parent)
: QGraphicsView(parent)
{
    setAcceptDrops(true);
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing |
                       QPainter::SmoothPixmapTransform);
}

HyperedgeView::HyperedgeView ( HyperedgeScene * scene, QWidget * parent)
: QGraphicsView(scene, parent)
{
    setAcceptDrops(true);
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing |
                       QPainter::SmoothPixmapTransform);
}

void HyperedgeView::wheelEvent(QWheelEvent *event)
{
    scaleView(pow(2.0, -event->delta() / 240.0));
}

void HyperedgeView::scaleView(qreal scaleFactor)
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

void HyperedgeView::mouseReleaseEvent(QMouseEvent* event)
{
    // always try to reset drag mode, just to be sure
    if (dragMode() != QGraphicsView::NoDrag) {
        setDragMode(QGraphicsView::NoDrag);
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void HyperedgeView::mousePressEvent(QMouseEvent* event)
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

HyperedgeEdit::HyperedgeEdit(QWidget *parent)
: HyperedgeView(parent)
{
    selectedItem = NULL;
    lineItem = NULL;
    isDrawLineMode = false;
    currentLabel = "Name?";
    isEditLabelMode = false;
}

HyperedgeEdit::HyperedgeEdit(HyperedgeScene * scene, QWidget * parent)
: HyperedgeView(scene, parent)
{
    selectedItem = NULL;
    lineItem = NULL;
    isDrawLineMode = false;
    currentLabel = "Name?";
    isEditLabelMode = false;
}

void HyperedgeEdit::keyPressEvent(QKeyEvent * event)
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
        auto edge = Hyperedge::find(selectedItem->getHyperEdgeId());
        if (edge)
        {
            edge->updateLabel(currentLabel.toStdString());
        }
        setDefaultLabel(currentLabel);
    }
    else if (selectedItem && isEditLabelMode)
    {
        // Update current label
        currentLabel += event->text();
        auto edge = Hyperedge::find(selectedItem->getHyperEdgeId());
        if (edge)
        {
            edge->updateLabel(currentLabel.toStdString());
        }
        setDefaultLabel(currentLabel);
    }

    QGraphicsView::keyPressEvent(event);
}

void HyperedgeEdit::mousePressEvent(QMouseEvent* event)
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
    HyperedgeView::mousePressEvent(event);
}

void HyperedgeEdit::mouseMoveEvent(QMouseEvent* event)
{
    // When in DRAW_LINE mode, update temporary visual line
    if (isDrawLineMode)
    {
        lineItem->setLine(sourceItem->pos().x(), sourceItem->pos().y(), mapToScene(event->pos()).x(), mapToScene(event->pos()).y());
    }

    QGraphicsView::mouseMoveEvent(event);
}

void HyperedgeEdit::mouseReleaseEvent(QMouseEvent* event)
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

    HyperedgeView::mouseReleaseEvent(event);
}

void HyperedgeEdit::setDefaultLabel(const QString& label)
{
    currentLabel = label;
    emit labelChanged(currentLabel);
}

HyperedgeViewer::HyperedgeViewer(QWidget *parent)
    : QWidget(parent)
{
    mpUi = new Ui::HyperedgeViewer();
    mpUi->setupUi(this);

    mpScene = new ForceBasedScene();
    mpView = new HyperedgeEdit(mpScene);
    mpView->show();
    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(mpView);
    mpUi->View->setLayout(layout);

    mpUi->usageLabel->setText("LMB: Select  RMB: Associate  WHEEL: Zoom  DEL: Delete  INS: Insert");

}

HyperedgeViewer::~HyperedgeViewer()
{
    delete mpView;
    delete mpScene;
    delete mpUi;
}

void HyperedgeViewer::loadFromYAMLFile(const QString& fileName)
{
    auto newEdges = YAML::load(YAML::LoadFile(fileName.toStdString())); // std::string >> YAML::Node >> Hyperedge::Hyperedges
    mpScene->visualize(newEdges);
}

void HyperedgeViewer::loadFromYAML(const QString& yamlString)
{
    auto newEdges = YAML::load(YAML::Load(yamlString.toStdString()));
    mpScene->visualize(newEdges);
}

void HyperedgeViewer::storeToYAML()
{
    std::stringstream result;
    result << YAML::store(mpScene->getAllEdges()); // Hyperedge::Hyperedges >> YAML::Node >> std::string
    emit YAMLStringReady(QString::fromStdString(result.str()));
}
