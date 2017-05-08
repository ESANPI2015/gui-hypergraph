#include "HyperedgeItem.hpp"
#include <QWidget>
#include <QPainter>
#include <QColor>
#include "Hyperedge.hpp"
#include <iostream>

HyperedgeItem::HyperedgeItem(Hyperedge *edge)
: mpEdge(edge)
{
    highlighted = false;
    setFlag(ItemIsMovable);
    setFlag(ItemSendsScenePositionChanges);
    setVisible(true);
    setZValue(1);
}

HyperedgeItem::~HyperedgeItem()
{
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
    for (auto edge : mEdgeVec)
    {
        edge->adjust();
    }
}

void HyperedgeItem::registerEdgeItem(EdgeItem *line)
{
    mEdgeVec.push_back(line);
}

void HyperedgeItem::deregisterEdgeItem(EdgeItem *line)
{
    int index = mEdgeVec.indexOf(line);
    if (index >= 0)
        mEdgeVec.remove(index);
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
    mLabelWidth = qMax(fm.width(mpEdge->label().c_str()), 10);
    mLabelHeight = qMax(fm.height(), 10);

    if (highlighted)
        painter->setBrush(Qt::yellow);
    else
        painter->setBrush(Qt::white);
    painter->drawRoundedRect(-mLabelWidth/2-5, -mLabelHeight/2-5, mLabelWidth+10, mLabelHeight+10, 5, 5);
    painter->drawText(-mLabelWidth/2, mLabelHeight/2-3, mpEdge->label().c_str());
}

EdgeItem::EdgeItem(HyperedgeItem *from, HyperedgeItem *to)
: mpSourceEdge(from),
  mpTargetEdge(to)
{
    from->registerEdgeItem(this);
    to->registerEdgeItem(this);
    setVisible(true);
}

EdgeItem::~EdgeItem()
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

    return QRectF(x-1,y-1,w+2,h+2);
}

void EdgeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
           QWidget *widget)
{
    painter->drawLine(mpSourceEdge->pos(), mpTargetEdge->pos());
}
