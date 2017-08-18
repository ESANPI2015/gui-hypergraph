#include "ConceptgraphItem.hpp"
#include <QWidget>
#include <QPainter>
#include <QColor>
#include <QStyleOptionGraphicsItem>
#include <QtCore>

ConceptgraphItem::ConceptgraphItem(Hyperedge *edge, ConceptgraphItemType type)
: HyperedgeItem(edge)
{
    mType = type;
}

ConceptgraphItem::~ConceptgraphItem()
{
}

void ConceptgraphItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
           QWidget *widget)
{
    if (isSelected())
        painter->setBrush(Qt::yellow);
    else
        painter->setBrush(Qt::white);

    switch (mType)
    {
        case RELATION:
            // do not draw boundary of rect
            painter->setPen(Qt::white);
            painter->drawRoundedRect(option->exposedRect, 5, 5);
            painter->setPen(Qt::black);
            break;
        default:
            painter->drawRoundedRect(option->exposedRect, 5, 5);
            break;
    }
    QGraphicsTextItem::paint(painter, option, widget);
}
