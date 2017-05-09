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
        HyperedgeItem(unsigned int id);
        virtual ~HyperedgeItem();

        QRectF boundingRect() const;

        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                   QWidget *widget);

        Hyperedge* getHyperEdge();
        unsigned int getHyperEdgeId()
        {
            return edgeId;
        }

        // Method to register edge items at the hyperedge item
        void registerEdgeItem(unsigned int otherId, EdgeItem *line);
        void deregisterEdgeItem(unsigned int otherId);

        // Tell all registered edge items to update themselves!!!
        void updateEdgeItems();

        // Set to true if item shall be highlighted
        void setHighlight(bool choice);
        bool isHighlighted();

        QMap<unsigned int, EdgeItem*> getEdgeItems()
        {
            return mEdgeMap;
        }

    protected:
        /*Callback to inform others when the item changed position or size*/
        virtual QVariant itemChange(GraphicsItemChange change, const QVariant& value);

    private:
        unsigned int edgeId;
        bool highlighted;

        int mLabelWidth;
        int mLabelHeight;
        QMap<unsigned int, EdgeItem*> mEdgeMap;
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

    private:
        HyperedgeItem* mpSourceEdge;
        HyperedgeItem* mpTargetEdge;
};

#endif


