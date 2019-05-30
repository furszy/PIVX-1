#ifndef DASHBOARDWIDGET_H
#define DASHBOARDWIDGET_H

#include "qt/pivx/pwidget.h"
#include "qt/pivx/furabstractlistitemdelegate.h"
#include "qt/pivx/furlistrow.h"
#include "transactiontablemodel.h"
#include "qt/pivx/txviewholder.h"
#include "transactionfilterproxy.h"

#include <iostream>
#include <cstdlib>

#include <QWidget>
#include <QLineEdit>

#include <QtCharts/QChartView>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QBarSet>
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>

QT_CHARTS_USE_NAMESPACE

using namespace QtCharts;

class PIVXGUI;
class WalletModel;

namespace Ui {
class DashboardWidget;
}

class SortEdit : public QLineEdit{
    Q_OBJECT
public:
    explicit SortEdit(QWidget* parent = nullptr) : QLineEdit(parent){}

    inline void mousePressEvent(QMouseEvent *) override{
        emit Mouse_Pressed();
    }

    ~SortEdit() override{}

signals:
    void Mouse_Pressed();

};

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

class DashboardWidget : public PWidget
{
    Q_OBJECT

public:
    explicit DashboardWidget(PIVXGUI* _window, QWidget *parent = nullptr);
    ~DashboardWidget();

    void loadWalletModel() override;
    void loadChart();

public slots:
    void walletSynced(bool isSync);
private slots:
    void windowResizeEvent(QResizeEvent *event);
    void handleTransactionClicked(const QModelIndex &index);
    void changeTheme(bool isLightTheme, QString &theme) override;
    void changeChartColors();
    void onSortTxPressed();
    void onSortChanged(const QString&);
    void updateDisplayUnit();
    void showList();
    void onTxArrived();
    void openFAQ();
private:
    Ui::DashboardWidget *ui;
    FurAbstractListItemDelegate* txViewDelegate;
    TransactionFilterProxy* filter;
    TransactionFilterProxy* stakesFilter;
    TxViewHolder* txHolder;
    TransactionTableModel* txModel;
    int nDisplayUnit = -1;

    bool isSync = false;

    // Chart
    QChartView *chartView = nullptr;
    QBarSeries *series = nullptr;
    QBarSet *set0 = nullptr;
    QBarSet *set1 = nullptr;

    QBarCategoryAxis *axisX = nullptr;
    QValueAxis *axisY = nullptr;

    QChart *chart = nullptr;
    bool isChartMin = false;
    int chartShow = 0;
    bool hasZpivStakes = false;

    void initChart();
    void refreshChart();
    QMap<int, std::pair<qint64, qint64>> getAmountBy();
    void updateAxisX(const char *arg[] = nullptr);
    std::pair<int, int> getChartRange();
};

#endif // DASHBOARDWIDGET_H
