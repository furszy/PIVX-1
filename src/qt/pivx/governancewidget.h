// Copyright (c) 2021 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef GOVERNANCEWIDGET_H
#define GOVERNANCEWIDGET_H

#include "qt/pivx/pwidget.h"
#include "qt/pivx/proposalcard.h"

#include <QGridLayout>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QStyledItemDelegate>
#include <QApplication>

namespace Ui {
class governancewidget;
}

class PIVXGUI;
class GovernanceModel;

class Delegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit Delegate(QObject *parent = nullptr) :
            QStyledItemDelegate(parent) {}

    void setValues(QList<QString> _values) {
        values = _values;
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        if (!index.isValid())
            return;

        QStyleOptionViewItem opt = option;
        QStyle *style = option.widget ? option.widget->style() : QApplication::style();
        opt.text = values.value(index.row());
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, option.widget);
    }

private:
    QList<QString> values;
};

class GovernanceWidget : public PWidget
{
    Q_OBJECT

public:
    explicit GovernanceWidget(PIVXGUI* parent);
    ~GovernanceWidget() override;

    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void loadClientModel() override;

public Q_SLOTS:
    void onVoteForPropClicked();

private:
    Ui::governancewidget *ui;
    GovernanceModel* governanceModel{nullptr};
    QGridLayout* gridLayout{nullptr}; // cards
    std::vector<ProposalCard*> cards;
    int propsPerRow = 0;

    void showEmptyScreen(bool show);
    void tryGridRefresh();
    ProposalCard* newCard();
    void refreshCardsGrid(bool forceRefresh);
    int calculateColumnsPerRow();
};

#endif // GOVERNANCEWIDGET_H
