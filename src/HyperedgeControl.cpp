#include "HyperedgeControl.hpp"
#include "ui_HyperedgeControl.h"

HyperedgeControl::HyperedgeControl(QWidget *parent)
{
    mpUi = new Ui::HyperedgeControl();
    mpUi->setupUi(this);
}

HyperedgeControl::~HyperedgeControl()
{
    delete mpUi;
}

void HyperedgeControl::on_loadButton_clicked()
{
    emit loadHyperedgeSystem();
}

void HyperedgeControl::on_saveButton_clicked()
{
    emit storeHyperedgeSystem();
}
