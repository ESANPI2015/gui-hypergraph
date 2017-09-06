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
    setLabel(QString::fromStdString(edge->label() + ":" + superClassLabel));
}

CommonConceptGraphItem::~CommonConceptGraphItem()
{
}

QRectF CommonConceptGraphItem::boundingRect() const
{
    return (childrenBoundingRect() | HyperedgeItem::boundingRect());
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
        case CONNECTOR:
            // draw an ellipse instead of a rectangle
            painter->drawEllipse(r);
            break;
        case CONTAINER:
        default:
            painter->drawRoundedRect(r, 5, 5);
            break;
    }
    QGraphicsTextItem::paint(painter, option, widget);
}
