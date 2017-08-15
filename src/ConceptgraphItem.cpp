#include "ConceptgraphItem.hpp"
#include <QWidget>
#include <QPainter>
#include <QColor>
#include <QtCore>

ConceptgraphItem::ConceptgraphItem(Hyperedge *edge, ConceptgraphItemType type)
: HyperedgeItem(edge)
{
    mType = type;
}

ConceptgraphItem::~ConceptgraphItem()
{
}

QRectF ConceptgraphItem::boundingRect() const
{
    return QRectF(-mLabelWidth/2-5-1, -mLabelHeight/2-5-1,
                  mLabelWidth + 10 + 2, mLabelHeight + 10 + 2);
}

void ConceptgraphItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
           QWidget *widget)
{
    QFontMetrics fm(widget->fontMetrics());
    mLabelWidth = qMax(fm.width(label), 20);
    mLabelHeight = qMax(fm.height(), 10);

    if (isSelected())
        painter->setBrush(Qt::yellow);
    else
        painter->setBrush(Qt::white);

    switch (mType)
    {
        case RELATION:
            // do not draw boundary of rect
            painter->setPen(Qt::white);
            painter->drawRoundedRect(-mLabelWidth/2-5, -mLabelHeight/2-5, mLabelWidth+10, mLabelHeight+10, 5, 5);
            painter->setPen(Qt::black);
            break;
        default:
            painter->drawRoundedRect(-mLabelWidth/2-5, -mLabelHeight/2-5, mLabelWidth+10, mLabelHeight+10, 5, 5);
            break;
    }
    painter->drawText(-mLabelWidth/2, mLabelHeight/2-3, label);
}
