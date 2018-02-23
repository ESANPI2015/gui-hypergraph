#include "CommonConceptGraphItem.hpp"
#include <QWidget>
#include <QPainter>
#include <QColor>
#include <QStyleOptionGraphicsItem>
#include <QtCore>
#include "Hyperedge.hpp"

CommonConceptGraphItem::CommonConceptGraphItem(Hyperedge *edge, CommonConceptGraphItemType type, std::string superClassLabel)
: HyperedgeItem(edge)
{
    mType = type;
    setLabel(QString::fromStdString(edge->label()), QString::fromStdString(superClassLabel));
}

CommonConceptGraphItem::~CommonConceptGraphItem()
{
}

QRectF CommonConceptGraphItem::boundingRect() const
{
    return (childrenBoundingRect() | HyperedgeItem::boundingRect());
}

void CommonConceptGraphItem::setLabel(const QString& l, const QString& cl)
{
    HyperedgeItem::setLabel(cl + "\n" + l);
}

void CommonConceptGraphItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
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

    switch (mType)
    {
        case INSTANCE:
            // draw an ellipse instead of a rectangle
            painter->drawRoundedRect(r, 10, 10);
            break;
        case CLASS:
        default:
            painter->drawRect(r);
            break;
    }
    QGraphicsTextItem::paint(painter, option, widget);
}

CommonConceptGraphEdgeItem::CommonConceptGraphEdgeItem(HyperedgeItem *from, HyperedgeItem *to, const Type type, const Style style)
: EdgeItem(from,to,type)
{
    mStyle=style;
}

CommonConceptGraphEdgeItem::~CommonConceptGraphEdgeItem()
{
}

void CommonConceptGraphEdgeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
           QWidget *widget)
{
    QPointF start(mpSourceEdge->centerPos());
    QPointF end(mpTargetEdge->centerPos());
    QPointF delta(end - start); // Points to end!
    float len_sqr = delta.x() * delta.x() + delta.y() * delta.y();
    if (!(len_sqr > 0.f))
        return;
    //float len = qSqrt(len_sqr);

    QPainterPath path;
    path.moveTo(start);
    switch (mStyle)
    {
        case SOLID_CURVED:
            {
            // Draw bezier curve
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
            path.cubicTo(c1, c2, end);
            painter->setPen(Qt::SolidLine);
            }
            break;
        case SOLID_STRAIGHT:
            painter->setPen(Qt::SolidLine);
            path.lineTo(end);
            break;
        case DASHED_STRAIGHT:
            painter->setPen(Qt::DashLine);
            path.lineTo(end);
            break;
        case DOTTED_STRAIGHT:
            painter->setPen(Qt::DotLine);
            path.lineTo(end);
            break;
    }
    painter->drawPath(path);
}
