#include "HypergraphControl.hpp"
#include "ui_HypergraphControl.h"

HypergraphControl::HypergraphControl(QWidget *parent)
{
    mpUi = new Ui::HypergraphControl();
    mpUi->setupUi(this);
}

HypergraphControl::~HypergraphControl()
{
    delete mpUi;
}

void HypergraphControl::on_loadButton_clicked()
{
    emit loadHypergraph();
}

void HypergraphControl::on_saveButton_clicked()
{
    emit storeHypergraph();
}

void HypergraphControl::on_clearButton_clicked()
{
    emit clearHypergraph();
}
