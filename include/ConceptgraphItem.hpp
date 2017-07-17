#ifndef _CONCEPTGRAPH_ITEM_HPP
#define _CONCEPTGRAPH_ITEM_HPP

#include <QGraphicsItem>
#include <QSet>
#include "HyperedgeItem.hpp"

class ConceptgraphItem : public HyperedgeItem
{
    public:
        typedef enum {
            CONCEPT,
            RELATION
        } ConceptgraphItemType;

        ConceptgraphItem(Hyperedge *x, ConceptgraphItemType type);
        virtual ~ConceptgraphItem();

        QRectF boundingRect() const;

        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                   QWidget *widget);

        ConceptgraphItemType getType()
        {
            return mType;
        }

    protected:
        ConceptgraphItemType mType;
};

#endif



