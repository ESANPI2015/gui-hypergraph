#include "HyperedgeViewer.hpp"
#include "ui_HyperedgeViewer.h"
#include "HyperedgeItem.hpp"

#include <QGraphicsScene>
#include <QWheelEvent>
#include <QTimer>
#include <QtCore>

#include "Hyperedge.hpp"
#include "HyperedgeYAML.hpp"
#include <iostream>

HyperedgeViewer::HyperedgeViewer(QWidget *parent)
    : QWidget(parent)
{
    mpUi = new Ui::HyperedgeViewer();
    mpUi->setupUi(this);

    mpScene = new ForceBasedScene();
    mpView = new HyperedgeView(mpScene);
    mpView->show();
    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(mpView);
    mpUi->View->setLayout(layout);

    auto test = YAML::LoadFile("test.yml");
    auto root  = YAML::load(test);
    mpScene->visualize(root);
}

void HyperedgeScene::visualize(Hyperedge *root)
{
    if (!root)
        return;

    // First clear the old scene
    clear();
    
    // Then make a traversal through the graph, creating the corresponding items while checking a possible previous layout
    Hyperedge *traversal = root->traversal<Hyperedge>
              (
               [&](Hyperedge *x){
                    // Create a hyperedge item
                    HyperedgeItem *item;
                    if (currentItems.contains(x))
                    {
                        item = currentItems[x];
                    } else {
                        item = new HyperedgeItem(x);
                    }

                    // Set cached position or assign a new one!
                    if (currentLayout.contains(x)) {
                        item->setPos(currentLayout[x]);
                    } else {
                        item->setPos(qrand() % 2000 - 1000, qrand() % 2000 - 1000);
                    }

                    // Update current layout
                    currentLayout[x] = item->pos();
                    currentItems[x] = item;
                    addItem(item);
                    return false;
                },
               [](Hyperedge *x, Hyperedge *y){
                    // NOTE: We cannot draw lines here, because we dont know the other objects we point to yet
                    return true;
                },
               "visualizeFrom",
               Hyperedge::TraversalDirection::BOTH
              );

    // Now we have to go through all HyperedgeItems and create a EdgeItem for all things they point to
    QMap<Hyperedge*,HyperedgeItem*>::const_iterator it = currentItems.begin();
    while (it != currentItems.end())
    {
        auto edge = it.key();
        auto srcItem = it.value();
        for (auto otherId : edge->pointingTo())
        {
            auto other = Hyperedge::find(otherId);
            auto destItem = currentItems[other];
            // Create line
            auto line = new EdgeItem(srcItem, destItem);
            addItem(line);
        }
        ++it;
    }
    delete traversal;
}

HyperedgeViewer::~HyperedgeViewer()
{
    delete mpView;
    delete mpScene;
    delete mpUi;
}

HyperedgeView::HyperedgeView(QWidget *parent)
: QGraphicsView(parent)
{
    selectedItem = NULL;
    lineItem = NULL;
    isDrawLineMode = false;
    setAcceptDrops(true);
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing |
                       QPainter::SmoothPixmapTransform);
}

HyperedgeView::HyperedgeView ( HyperedgeScene * scene, QWidget * parent)
: QGraphicsView(scene, parent)
{
    selectedItem = NULL;
    lineItem = NULL;
    isDrawLineMode = false;
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

void HyperedgeView::keyPressEvent(QKeyEvent * event)
{
    if (event->key() == Qt::Key_Delete)
    {
        if (selectedItem)
        {
            // Delete edge from graph
            // TODO: This is really tricky ... Should halt the ForceBasedLayouter!!!
            // Furthermore, this operation could make a forest out of a single connected graph
        }
    }
    QGraphicsView::keyPressEvent(event);
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
        if (item)
        {
            HyperedgeItem *edge = dynamic_cast<HyperedgeItem*>(item);
            if (edge)
            {
                if (selectedItem)
                    selectedItem->setHighlight(false);
                selectedItem = edge;
                selectedItem->setHighlight(true);
            } else {
                setDragMode(QGraphicsView::ScrollHandDrag);
                if (selectedItem)
                    selectedItem->setHighlight(false);
                selectedItem = NULL;
            }
        } else {
            setDragMode(QGraphicsView::ScrollHandDrag);
            if (selectedItem)
                selectedItem->setHighlight(false);
            selectedItem = NULL;
        }
    }

    if (event->button() == Qt::RightButton)
    {
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
    QGraphicsView::mousePressEvent(event);
}

void HyperedgeView::mouseMoveEvent(QMouseEvent* event)
{
    // When in DRAW_LINE mode, update temporary visual line
    if (isDrawLineMode)
    {
        lineItem->setLine(sourceItem->pos().x(), sourceItem->pos().y(), mapToScene(event->pos()).x(), mapToScene(event->pos()).y());
    }

    QGraphicsView::mouseMoveEvent(event);
}

void HyperedgeView::mouseReleaseEvent(QMouseEvent* event)
{
    QGraphicsItem *item = itemAt(event->pos());
    // always try to reset drag mode, just to be sure
    if (dragMode() != QGraphicsView::NoDrag) {
        setDragMode(QGraphicsView::NoDrag);
    }

    if ((event->button() == Qt::RightButton) && isDrawLineMode)
    {
        // Check if there is a item at current pos
        auto edge = dynamic_cast<HyperedgeItem*>(item);
        if (edge)
        {
            // Add model edge
            if (sourceItem->getHyperEdge()->pointTo(edge->getHyperEdge()->id()))
            {
                // Add visual edge as well
                auto line = new EdgeItem(sourceItem, edge);
                scene()->addItem(line);
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

    QGraphicsView::mouseReleaseEvent(event);
}

ForceBasedScene::ForceBasedScene(QObject * parent)
: HyperedgeScene(parent)
{
    mpTimer = new QTimer(this);
    connect(mpTimer, SIGNAL(timeout()), this, SLOT(updateLayout()));
    mpTimer->start(1000/25);

    mEquilibriumDistance = 100;
}

ForceBasedScene::~ForceBasedScene()
{
    delete mpTimer;
}

HyperedgeScene::HyperedgeScene(QObject * parent)
{
}

HyperedgeScene::~HyperedgeScene()
{
}


void ForceBasedScene::updateLayout()
{
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

        edge->setPos(item->pos() + displacements[edge] / 1000.);
    }
}
