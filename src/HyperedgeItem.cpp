#include "HyperedgeItem.hpp"
#include <QWidget>
#include <QPainter>
#include <QColor>
#include <QtCore>
#include "Hyperedge.hpp"

HyperedgeItem::HyperedgeItem(const unsigned int id, const QString& l)
: edgeId(id),
  label(l)
{
    mLabelHeight = 10;
    mLabelWidth = 20;
    highlighted = false;
    setFlag(ItemIsMovable);
    setFlag(ItemSendsScenePositionChanges);
    setVisible(true);
    setZValue(1);
}

HyperedgeItem::HyperedgeItem(Hyperedge *edge)
: edgeId(edge->id()),
  label(QString::fromStdString(edge->label()))
{
    mLabelHeight = 10;
    mLabelWidth = 20;
    highlighted = false;
    setFlag(ItemIsMovable);
    setFlag(ItemSendsScenePositionChanges);
    setVisible(true);
    setZValue(1);
}

HyperedgeItem::~HyperedgeItem()
{
}

void HyperedgeItem::setLabel(const QString& l)
{
    prepareGeometryChange();
    label = l;
}

void HyperedgeItem::setHighlight(bool choice)
{
    prepareGeometryChange();
    highlighted = choice;
}

bool HyperedgeItem::isHighlighted()
{
    return highlighted;
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
    return QGraphicsItem::itemChange(change, value);
}

QRectF HyperedgeItem::boundingRect() const
{
    return QRectF(-mLabelWidth/2-5-1, -mLabelHeight/2-5-1,
                  mLabelWidth + 10 + 2, mLabelHeight + 10 + 2);
}

void HyperedgeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
           QWidget *widget)
{
    QFontMetrics fm(widget->fontMetrics());
    mLabelWidth = qMax(fm.width(label), 20);
    mLabelHeight = qMax(fm.height(), 10);

    if (highlighted)
        painter->setBrush(Qt::yellow);
    else
        painter->setBrush(Qt::white);
    painter->drawRoundedRect(-mLabelWidth/2-5, -mLabelHeight/2-5, mLabelWidth+10, mLabelHeight+10, 5, 5);
    painter->drawText(-mLabelWidth/2, mLabelHeight/2-3, label);
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
    QPointF a(mpSourceEdge->pos());
    QPointF b(mpTargetEdge->pos());

    float x = qMin(a.x(), b.x());
    float y = qMin(a.y(), b.y());
    float w = qAbs(qMax(a.x(), b.x()) - x);    
    float h = qAbs(qMax(a.y(), b.y()) - y);    

    return QRectF(x-5,y-5,w+10,h+10);
}

void EdgeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
           QWidget *widget)
{
    painter->drawLine(mpSourceEdge->pos(), mpTargetEdge->pos());
    // Draw the circle only for TO edges
    if (mType == TO)
    {
        QPointF delta(mpTargetEdge->pos() - mpSourceEdge->pos());
        float len = qSqrt(delta.x() * delta.x() + delta.y() * delta.y() + 1);
        QPointF circlePos(mpTargetEdge->pos() - delta / len * 40);
        QRectF rect(circlePos.x()-5, circlePos.y()-5, 10, 10);
        painter->setBrush(Qt::black);
        painter->drawEllipse(rect);
    }
}
