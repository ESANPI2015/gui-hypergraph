#ifndef _HYPEREDGE_ITEM_HPP
#define _HYPEREDGE_ITEM_HPP

#include <QGraphicsTextItem>
#include <QSet>
#include "Hyperedge.hpp"

class EdgeItem;
class QGraphicsTextItem;

class HyperedgeItem : public QGraphicsTextItem
{
    public:
        HyperedgeItem(const UniqueId& uid);
        virtual ~HyperedgeItem();

        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                   QWidget *widget);

        UniqueId getHyperEdgeId()
        {
            return edgeId;
        }

        // Method to register edge items at the hyperedge item
        void registerEdgeItem(EdgeItem *line);
        void deregisterEdgeItem(EdgeItem *line);

        // Tell all registered edge items to update themselves!!!
        // Furthermore adjust own boundingrect
        void updateEdgeItems();

        // Change the label
        void setLabel(const QString& l);

        QSet<EdgeItem*> getEdgeItems()
        {
            return mEdgeSet;
        }

        QPointF centerPos();

    protected:
        /*Callback to inform others when the item changed position or size*/
        virtual QVariant itemChange(GraphicsItemChange change, const QVariant& value);

        UniqueId edgeId;
        QSet<EdgeItem*> mEdgeSet;

        /*last pos added*/
        QPointF lastPosUsed;
};

class EdgeItem : public QGraphicsItem
{
    public:
        enum Type {
            TO,
            FROM
        };

        EdgeItem(HyperedgeItem *from, HyperedgeItem *to, const Type type=TO);
        virtual ~EdgeItem();

        QRectF boundingRect() const;

        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                   QWidget *widget);

        // This is called whenever this item has to change
        void adjust();

        // Call this to adjust the Z Value to be one less than the maximum value
        void findProperZ();

        // Deregisters from HyperedgeItems
        void deregister();

        HyperedgeItem* getSourceItem()
        {
            return mpSourceEdge;
        }
        HyperedgeItem* getTargetItem()
        {
            return mpTargetEdge;
        }
        Type getType()
        {
            return mType;
        }

    protected:
        HyperedgeItem* mpSourceEdge;
        HyperedgeItem* mpTargetEdge;
        Type mType;
};

#endif


