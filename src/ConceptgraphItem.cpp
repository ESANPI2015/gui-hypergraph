#include "ConceptgraphItem.hpp"
#include <QWidget>
#include <QPainter>
#include <QColor>
#include <QStyleOptionGraphicsItem>
#include <QtCore>

ConceptgraphItem::ConceptgraphItem(const UniqueId& uid, ConceptgraphItemType type)
: HyperedgeItem(uid)
{
    mType = type;
}

ConceptgraphItem::~ConceptgraphItem()
{
}

void ConceptgraphItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
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
        case RELATION:
            // do not draw boundary of rect
            painter->setPen(Qt::white);
            painter->drawRoundedRect(r, 5, 5);
            painter->setPen(Qt::black);
            break;
        default:
            painter->drawRoundedRect(r, 5, 5);
            break;
    }
    QGraphicsTextItem::paint(painter, option, widget);
}
