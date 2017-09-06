#ifndef _COMMONCONCEPTGRAPH_ITEM_HPP
#define _COMMONCONCEPTGRAPH_ITEM_HPP

#include <QGraphicsItem>
#include <QSet>
#include "HyperedgeItem.hpp"

class CommonConceptGraphItem : public HyperedgeItem
{
    public:
        typedef enum {
            NORMAL,
            CONTAINER,
            CONNECTOR
        } CommonConceptGraphItemType;

        CommonConceptGraphItem(Hyperedge *x,
                               CommonConceptGraphItemType type,
                               std::string superClassLabel
                              );
        virtual ~CommonConceptGraphItem();

        QRectF boundingRect() const;

        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                   QWidget *widget);

        CommonConceptGraphItemType getType()
        {
            return mType;
        }

    protected:
        CommonConceptGraphItemType mType;
};

#endif



