#ifndef _HYPEREDGE_ITEM_HPP
#define _HYPEREDGE_ITEM_HPP

#include <QGraphicsItem>
#include <QVector>

class Hyperedge;
class EdgeItem;

class HyperedgeItem : public QGraphicsItem
{
    public:
        HyperedgeItem(Hyperedge* edge);
        virtual ~HyperedgeItem();

        QRectF boundingRect() const;

        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                   QWidget *widget);

        Hyperedge* getHyperEdge()
        {
            return mpEdge;
        }

        // Method to register edge items at the hyperedge item
        void registerEdgeItem(EdgeItem *line);
        void deregisterEdgeItem(EdgeItem *line);

        // Tell all registered edge items to update themselves!!!
        void updateEdgeItems();

        // Set to true if item shall be highlighted
        void setHighlight(bool choice);
        bool isHighlighted();

        QVector<EdgeItem*> getEdgeItems()
        {
            return mEdgeVec;
        }

    protected:
        /*Callback to inform others when the item changed position or size*/
        virtual QVariant itemChange(GraphicsItemChange change, const QVariant& value);

    private:
        bool highlighted;
        Hyperedge* mpEdge;

        int mLabelWidth;
        int mLabelHeight;
        QVector<EdgeItem*> mEdgeVec;
};

class EdgeItem : public QGraphicsItem
{
    public:
        EdgeItem(HyperedgeItem *from, HyperedgeItem *to);
        virtual ~EdgeItem();

        QRectF boundingRect() const;

        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                   QWidget *widget);

        // This is called whenever this item has to change
        void adjust();

        HyperedgeItem* getSourceItem()
        {
            return mpSourceEdge;
        }
        HyperedgeItem* getTargetItem()
        {
            return mpTargetEdge;
        }

    private:
        HyperedgeItem* mpSourceEdge;
        HyperedgeItem* mpTargetEdge;
};

#endif


