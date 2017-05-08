#include "HyperedgeGUI.hpp"
#include "ui_HyperedgeGUI.h"

#include "HyperedgeViewer.hpp"

HyperedgeGUI::HyperedgeGUI(QWidget *parent)
    : QMainWindow(parent)
{
    mpUi = new Ui::HyperedgeGUI();
    mpUi->setupUi(this);

    mpViewer = new HyperedgeViewer();
    setCentralWidget(mpViewer);
}

HyperedgeGUI::~HyperedgeGUI()
{
    delete mpUi;
}
