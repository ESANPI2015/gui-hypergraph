#ifndef _CONCEPTGRAPH_VIEWER_HPP
#define _CONCEPTGRAPH_VIEWER_HPP

#include "Conceptgraph.hpp"
#include "HypergraphViewer.hpp"

class ConceptgraphItem;

class ConceptgraphScene : public ForceBasedScene
{
    Q_OBJECT

    public:
        ConceptgraphScene(QObject * parent = 0);
        ~ConceptgraphScene();

        void addItem(QGraphicsItem *item);
        void removeItem(QGraphicsItem *item);
        Conceptgraph* graph()
        {
            return static_cast<Conceptgraph*>(currentGraph);
        }
        QList<ConceptgraphItem*> selectedConceptgraphItems();

    signals:
        void conceptAdded(const UniqueId id);
        void conceptRemoved(const UniqueId id);
        void relationAdded(const UniqueId id);
        void relationRemoved(const UniqueId id);

    public slots:
        void visualize(Conceptgraph* graph=NULL);
        void addConcept(const UniqueId id, const QString& label);
        void addRelation(const UniqueId fromId, const UniqueId toId, const UniqueId id, const QString& label);
        void removeEdge(const UniqueId id);
        void updateEdge(const UniqueId id, const QString& label);
};

class ConceptgraphEditor : public HypergraphEdit
{
    Q_OBJECT

    public:
        ConceptgraphEditor(QWidget *parent = 0);
        ConceptgraphEditor(ConceptgraphScene * scene, QWidget * parent = 0 );

        ConceptgraphScene* scene()
        {
            return dynamic_cast<ConceptgraphScene*>(QGraphicsView::scene());
        }

    protected:
        void keyPressEvent(QKeyEvent * event);
        void mousePressEvent(QMouseEvent* event);
        void mouseReleaseEvent(QMouseEvent* event);
};

class ConceptgraphWidget : public HypergraphViewer
{
    Q_OBJECT

    public:
        ConceptgraphWidget(QWidget *parent = 0);
        ~ConceptgraphWidget();

    public slots:
        void loadFromYAMLFile(const QString& fileName);
        void loadFromYAML(const QString& yamlString);
        void loadFromGraph(Conceptgraph& graph);
        void onGraphChanged(const UniqueId id);

    protected:
        // Triggered when widget is about to get visible
        void showEvent(QShowEvent *event);

        ConceptgraphScene*       mpConceptScene;
        ConceptgraphEditor*      mpConceptEditor;
};

#endif


