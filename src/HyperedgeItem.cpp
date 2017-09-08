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
    QPointF topLeft(scenePos());
    QRectF  rect(childrenBoundingRect() | boundingRect());
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
            // If we are part of a parent item we have to call its update func
            if (parentItem())
            {
                HyperedgeItem* trueParent = dynamic_cast<HyperedgeItem*>(parentItem());
                if (trueParent)
                    trueParent->updateEdgeItems();
            }
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
    QRectF r = boundingRect();
    QPen p = painter->pen();

    r.setWidth(r.width() - p.width());
    r.setHeight(r.height() - p.width());

    if (isSelected())
        painter->setBrush(Qt::yellow);
    else
        painter->setBrush(Qt::white);
    painter->drawRoundedRect(r, 5, 5);
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
    float len_sqr = delta.x() * delta.x() + delta.y() * delta.y();
    if (!(len_sqr > 0.f))
        return;
    float len = qSqrt(len_sqr);
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

    // Draw a directional identifier only if it would be visible
    QRectF targetRect(mpTargetEdge->boundingRect());
    float maxR_sqr = (targetRect.width() * targetRect.width() + targetRect.height() * targetRect.height()) / 4.;
    if (maxR_sqr > len_sqr)
        return;

    // Calculate the maximum radius at which a direction identifier should be placed
    float maxR = qSqrt(maxR_sqr);
    QPointF circlePos(mpTargetEdge->centerPos() - delta / len * maxR);
    QRectF rect(circlePos.x()-5, circlePos.y()-5, 10, 10);
    painter->drawEllipse(rect);
}
