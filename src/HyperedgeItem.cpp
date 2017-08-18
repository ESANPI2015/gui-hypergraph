#include "HyperedgeItem.hpp"
#include <QWidget>
#include <QPainter>
#include <QColor>
#include <QStyleOptionGraphicsItem>
#include <QtCore>
#include "Hyperedge.hpp"

HyperedgeItem::HyperedgeItem(Hyperedge *edge)
: edgeId(edge->id())
{
    setFlag(ItemIsMovable);
    setFlag(ItemIsSelectable);
    setFlag(ItemSendsScenePositionChanges);
    setVisible(true);
    setZValue(1);
    setPlainText(QString::fromStdString(edge->label()));
}

HyperedgeItem::~HyperedgeItem()
{
}

QPointF HyperedgeItem::centerPos()
{
    QPointF topLeft(pos());
    QRectF  rect(boundingRect());
    return (QPointF(topLeft.x() + rect.width()/2., topLeft.y() + rect.height()/2.));
}

void HyperedgeItem::setLabel(const QString& l)
{
    setPlainText(l);
}

void HyperedgeItem::updateEdgeItems()
{
    prepareGeometryChange();
    for (auto edge : mEdgeSet)
    {
        edge->adjust();
    }
}

void HyperedgeItem::registerEdgeItem(EdgeItem *line)
{
    if (!mEdgeSet.contains(line))
    {
        mEdgeSet.insert(line);
    }
}

void HyperedgeItem::deregisterEdgeItem(EdgeItem *line)
{
    mEdgeSet.remove(line);
}

QVariant HyperedgeItem::itemChange(GraphicsItemChange change,
                                    const QVariant& value)
{
    switch(change)
    {
        case ItemScenePositionHasChanged:
        {
            // When this happens, we have to redraw all the connections to/from other items
            updateEdgeItems();
            break;
        }
        default:
        {
            break;
        }
    };
    return QGraphicsTextItem::itemChange(change, value);
}

void HyperedgeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
           QWidget *widget)
{
    if (isSelected())
        painter->setBrush(Qt::yellow);
    else
        painter->setBrush(Qt::white);
    painter->drawRoundedRect(option->exposedRect, 5, 5);
    QGraphicsTextItem::paint(painter, option, widget);
}

EdgeItem::EdgeItem(HyperedgeItem *from, HyperedgeItem *to, const Type type)
: mpSourceEdge(from),
  mpTargetEdge(to),
  mType(type)
{
    from->registerEdgeItem(this);
    to->registerEdgeItem(this);
    setVisible(true);
}

EdgeItem::~EdgeItem()
{
}

void EdgeItem::deregister()
{
    mpSourceEdge->deregisterEdgeItem(this);
    mpTargetEdge->deregisterEdgeItem(this);
}

void EdgeItem::adjust()
{
    prepareGeometryChange();
}

QRectF EdgeItem::boundingRect() const
{
    // NOTE: Just using two points is not OK!
    QPointF a(mpSourceEdge->centerPos());
    QPointF b(mpTargetEdge->centerPos());

    float x = qMin(a.x(), b.x());
    float y = qMin(a.y(), b.y());
    float w = qAbs(qMax(a.x(), b.x()) - x);    
    float h = qAbs(qMax(a.y(), b.y()) - y);    

    return QRectF(x-5,y-5,w+10,h+10);
}

void EdgeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
           QWidget *widget)
{
    // Get the length and the vector between source and target
    QPointF delta(mpTargetEdge->centerPos() - mpSourceEdge->centerPos());
    float len = qSqrt(delta.x() * delta.x() + delta.y() * delta.y() + 1);

    // Calculate the maximum radius at which a direction identifier should be placed
    QRectF targetRect(mpTargetEdge->boundingRect());
    float maxR = qSqrt(targetRect.width() * targetRect.width() + targetRect.height() * targetRect.height()) / 2;
    QPointF circlePos(mpTargetEdge->centerPos() - delta / len * maxR);
    QRectF rect(circlePos.x()-5, circlePos.y()-5, 10, 10);

    // Decide how to draw circle
    if (mType == TO)
    {
        // Draw the circle black for TO edges
        painter->setBrush(Qt::black);
    } else {
        // or white otherwise
        painter->setBrush(Qt::white);
    }

    // paint
    painter->drawLine(mpSourceEdge->centerPos(), mpTargetEdge->centerPos());
    painter->drawEllipse(rect);
}
