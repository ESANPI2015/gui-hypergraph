#ifndef _COMMONCONCEPTGRAPH_VIEWER_HPP
#define _COMMONCONCEPTGRAPH_VIEWER_HPP

#include "CommonConceptGraph.hpp"
#include "ConceptgraphViewer.hpp"

// Generated by MOC
namespace Ui
{
    class CommonConceptGraphViewer;
}

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
        bool classesShown() { return mShowClasses; }
        bool instancesShown() { return mShowInstances; }

    signals:
        void classAdded(const UniqueId id);
        void classRemoved(const UniqueId id);
        void instanceAdded(const UniqueId id);
        void instanceRemoved(const UniqueId id);
        void relationAdded(const UniqueId id);
        void relationRemoved(const UniqueId id);

    public slots:
        void visualize(CommonConceptGraph* graph=NULL);
        void addInstance(const UniqueId superId, const QString& label);
        void addClass(const UniqueId id, const QString& label);
        void addRelationIsA(const UniqueId fromId, const UniqueId toId);
        void addRelationHasA(const UniqueId fromId, const UniqueId toId);
        void addRelationConnects(const UniqueId fromId, const UniqueId toId);
        //void addRelationPartOf(const UniqueId fromId, const UniqueId toId);
        void removeEdge(const UniqueId id);
        void updateEdge(const UniqueId id, const QString& label);
        void showClasses(const bool value);
        void showInstances(const bool value);

    protected:
        bool mShowClasses;
        bool mShowInstances;
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
        CommonConceptGraphWidget(QWidget *parent = 0, bool doSetup=true);
        ~CommonConceptGraphWidget();

    public slots:
        void loadFromYAMLFile(const QString& fileName);
        void loadFromYAML(const QString& yamlString);
        void loadFromGraph(CommonConceptGraph& graph);
        void onGraphChanged(const UniqueId id);

    protected:
        // Triggered when widget is about to get visible
        void showEvent(QShowEvent *event);

        CommonConceptGraphScene*       mpCommonConceptScene;
        CommonConceptGraphEditor*      mpCommonConceptEditor;
        Ui::CommonConceptGraphViewer*  mpNewUi;
};

#endif


