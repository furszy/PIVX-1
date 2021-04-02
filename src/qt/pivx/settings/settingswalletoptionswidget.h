// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SETTINGSWALLETOPTIONSWIDGET_H
#define SETTINGSWALLETOPTIONSWIDGET_H

#include <QWidget>
#include <QDataWidgetMapper>
#include "qt/pivx/pwidget.h"
namespace Ui {
class SettingsWalletOptionsWidget;
}

class SettingsWidget;

class SettingsWalletOptionsWidget : public PWidget
{
    Q_OBJECT

public:
    explicit SettingsWalletOptionsWidget(PIVXGUI* _window, QWidget *parent = nullptr);
    ~SettingsWalletOptionsWidget();

Q_SIGNALS:
    void saveSettings();
    void discardSettings();

public Q_SLOTS:
    void onResetClicked();

private:
    friend class SettingsWidget;

    Ui::SettingsWalletOptionsWidget *ui;

    void setMapper(QDataWidgetMapper *mapper);
    void setWalletModel(WalletModel* model);
    void reloadWalletOptions();

    void setSpinBoxStakeSplitThreshold(double val);
    double getSpinBoxStakeSplitThreshold() const;
};

#endif // SETTINGSWALLETOPTIONSWIDGET_H
