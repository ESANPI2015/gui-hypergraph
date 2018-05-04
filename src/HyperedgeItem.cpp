#include "HyperedgeItem.hpp"
#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QColor>
#include <QStyleOptionGraphicsItem>
#include <QtCore>
#include "Hyperedge.hpp"
#include <iostream>

HyperedgeItem::HyperedgeItem(Hyperedge *edge)
: edgeId(edge->id())
{
    setFlag(ItemIsMovable);
    setFlag(ItemIsSelectable);
    setFlag(ItemSendsScenePositionChanges);
    setVisible(true);
    setZValue(qrand());
    setPlainText(QString::fromStdString(edge->label()));
    lastPosUsed = QPointF(0.f,60.f);
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
    setPlainText(QString::fromStdString(edgeId) + "\n" + l);
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
        case ItemChildAddedChange:
        {
            QGraphicsItem* child = qvariant_cast<QGraphicsItem*>(value);
            child->setPos(lastPosUsed);
            lastPosUsed.setY(lastPosUsed.y() + 60.f);
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
    mpSourceEdge = NULL;
    mpTargetEdge = NULL;
}

void EdgeItem::adjust()
{
    prepareGeometryChange();
}

void EdgeItem::findProperZ()
{
    if (!mpSourceEdge || !mpTargetEdge)
        return;
    qreal minZ(qMin(mpSourceEdge->zValue(), mpTargetEdge->zValue()));
    setZValue(minZ-0.1f);
}

QRectF EdgeItem::boundingRect() const
{
    if (!mpSourceEdge || !mpTargetEdge)
        return QRectF();
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
    if (!mpSourceEdge || !mpTargetEdge)
        return;
    QPointF start(mpSourceEdge->centerPos());
    QPointF end(mpTargetEdge->centerPos());
    QPointF delta(end - start); // Points to end!
    float len_sqr = delta.x() * delta.x() + delta.y() * delta.y();
    if (!(len_sqr > 0.f))
        return;
    float len = qSqrt(len_sqr);

    // Draw bezier curve
    QPainterPath path;
    QPointF c1, c2;
    if (qAbs(delta.y()) < qAbs(delta.x()))
    {
        c1 = QPointF(end.x(), start.y());
        c2 = QPointF(start.x(), end.y());
    }
    else
    {
        c1 = QPointF(start.x(), end.y());
        c2 = QPointF(end.x(), start.y());
    }
    path.moveTo(start);
    path.cubicTo(c1, c2, end);
    painter->drawPath(path);
    findProperZ();
    // Decide how to draw circle
    if (mType == TO)
    {
        // TODO: The TO-edge shall get an arrow! We need 4 different cases of an arrow!
        QRectF targetRect(mpTargetEdge->boundingRect());
        float maxR_sqr = (targetRect.width() * targetRect.width() + targetRect.height() * targetRect.height()) / 4.;
        if (maxR_sqr > len_sqr)
            return;

        // Calculate the maximum radius at which a direction identifier should be placed
        float maxR = qSqrt(maxR_sqr);
        QPointF circlePos(end - delta / len * maxR);
        QRectF rect(circlePos.x()-5, circlePos.y()-5, 10, 10);
        painter->drawEllipse(rect);
    }
}
