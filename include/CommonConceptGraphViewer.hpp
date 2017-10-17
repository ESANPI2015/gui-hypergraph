#ifndef _COMMONCONCEPTGRAPH_VIEWER_HPP
#define _COMMONCONCEPTGRAPH_VIEWER_HPP

#include "CommonConceptGraph.hpp"
#include "ConceptgraphViewer.hpp"

class CommonConceptGraphItem;

class CommonConceptGraphScene : public ConceptgraphScene
{
    Q_OBJECT

    public:
        CommonConceptGraphScene(QObject * parent = 0);
        ~CommonConceptGraphScene();

        void addItem(QGraphicsItem *item);
        void removeItem(QGraphicsItem *item);
        CommonConceptGraph* graph()
        {
            return static_cast<CommonConceptGraph*>(currentGraph);
        }
        QList<CommonConceptGraphItem*> selectedCommonConceptGraphItems();

    signals:
        void conceptAdded(const unsigned id);
        void conceptRemoved(const unsigned id);
        void relationAdded(const unsigned id);
        void relationRemoved(const unsigned id);

    public slots:
        void visualize(CommonConceptGraph* graph=NULL);
        void addConcept(const unsigned id, const QString& label);
        void addRelation(const unsigned fromId, const unsigned toId, const unsigned id, const QString& label);
        void removeEdge(const unsigned id);
        void updateEdge(const unsigned int id, const QString& label);
};

class CommonConceptGraphEditor : public ConceptgraphEditor
{
    Q_OBJECT

    public:
        CommonConceptGraphEditor(QWidget *parent = 0);
        CommonConceptGraphEditor(CommonConceptGraphScene * scene, QWidget * parent = 0 );

        CommonConceptGraphScene* scene()
        {
            return dynamic_cast<CommonConceptGraphScene*>(QGraphicsView::scene());
        }

    protected:
        void keyPressEvent(QKeyEvent * event);
        void mousePressEvent(QMouseEvent* event);
        void mouseReleaseEvent(QMouseEvent* event);
};

class CommonConceptGraphWidget : public ConceptgraphWidget
{
    Q_OBJECT

    public:
        CommonConceptGraphWidget(QWidget *parent = 0);
        ~CommonConceptGraphWidget();

    public slots:
        void loadFromYAMLFile(const QString& fileName);
        void loadFromYAML(const QString& yamlString);
        void loadFromGraph(CommonConceptGraph& graph);
        void onGraphChanged(const unsigned id);

    protected:
        // Triggered when widget is about to get visible
        void showEvent(QShowEvent *event);

        CommonConceptGraphScene*       mpCommonConceptScene;
        CommonConceptGraphEditor*      mpCommonConceptEditor;
};

#endif

