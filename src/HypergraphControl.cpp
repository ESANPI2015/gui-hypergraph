#include "HypergraphControl.hpp"
#include "ui_HypergraphControl.h"

HypergraphControl::HypergraphControl(QWidget *parent)
{
    mpUi = new Ui::HypergraphControl();
    mpUi->setupUi(this);

    // Fill the list of possible graph types
    mpUi->typeBox->addItem("Hypergraph",QVariant(HypergraphType::HYPERGRAPH));
    mpUi->typeBox->addItem("Concept Graph",QVariant(HypergraphType::CONCEPTGRAPH));
    mpUi->typeBox->addItem("Common Concept Graph",QVariant(HypergraphType::COMMONCONCEPTGRAPH));
}

HypergraphControl::~HypergraphControl()
{
    delete mpUi;
}

void HypergraphControl::on_loadButton_clicked()
{
    emit loadHypergraph(static_cast<HypergraphType>(mpUi->typeBox->itemData(mpUi->typeBox->currentIndex()).toUInt()));
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
    emit newHypergraph(static_cast<HypergraphType>(mpUi->typeBox->itemData(mpUi->typeBox->currentIndex()).toUInt()));
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
