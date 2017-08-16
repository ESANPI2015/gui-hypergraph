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

void HypergraphControl::on_newButton_clicked()
{
    emit newHypergraph();
}

void HypergraphControl::on_equiSlider_valueChanged(int value)
{
    on_equiBox_valueChanged(value);
}

void HypergraphControl::on_equiBox_valueChanged(int value)
{
    mpUi->equiSlider->setValue(value);
    mpUi->equiBox->setValue(value);
    emit setEquilibriumDistance(1. * value);
}
