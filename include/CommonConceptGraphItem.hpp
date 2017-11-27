#ifndef _COMMONCONCEPTGRAPH_ITEM_HPP
#define _COMMONCONCEPTGRAPH_ITEM_HPP

#include <QGraphicsItem>
#include <QSet>
#include "HyperedgeItem.hpp"

class CommonConceptGraphItem : public HyperedgeItem
{
    public:
        typedef enum {
            CLASS,
            INSTANCE
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

        void setLabel(const QString& l, const QString& cl);

    protected:
        CommonConceptGraphItemType mType;
};

class CommonConceptGraphEdgeItem : public EdgeItem
{
    public:
        // TODO: Make this mirror the different relation types, not define styles!
        enum Style {
            SOLID_STRAIGHT,
            DASHED_STRAIGHT,
            DOTTED_STRAIGHT,
            SOLID_CURVED
        };
        CommonConceptGraphEdgeItem(HyperedgeItem *from, HyperedgeItem *to, const Type type=TO, const Style style=SOLID_CURVED);
        virtual ~CommonConceptGraphEdgeItem();

        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                   QWidget *widget);

        Style getStyle()
        {
            return mStyle;
        }

    protected:
        Style mStyle;
};

#endif



