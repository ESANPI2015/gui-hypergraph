#ifndef _HYPEREDGE_GUI_HPP
#define _HYPEREDGE_GUI_HPP

#include <QMainWindow>

// Generated by MOC
namespace Ui
{
    class HyperedgeGUI;
}

// Forward declarations
class HyperedgeViewer;

class HyperedgeGUI : public QMainWindow
{
    Q_OBJECT

    public:
        HyperedgeGUI(QWidget *parent = 0);
        ~HyperedgeGUI();

    private:
        Ui::HyperedgeGUI* mpUi;

        // Add hyperedge viewer
        HyperedgeViewer* mpViewer;
};

#endif
