//
// Created by furszy on 3/21/19.
//

#ifndef PIVX_CORE_NEW_GUI_PIVXGUI_H
#define PIVX_CORE_NEW_GUI_PIVXGUI_H

#if defined(HAVE_CONFIG_H)
#include "config/pivx-config.h"
#endif

#include <QMainWindow>
#include <QStackedWidget>
#include <QSystemTrayIcon>

#include "qt/pivx/navmenuwidget.h"
#include "qt/pivx/topbar.h"
#include "qt/pivx/dashboardwidget.h"
#include "qt/pivx/send.h"


class ClientModel;
class NetworkStyle;
class Notificator;
class WalletModel;


/**
  PIVX GUI main class. This class represents the main window of the PIVX UI. It communicates with both the client and
  wallet models to give the user an up-to-date view of the current core state.
*/
class PIVXGUI : public QMainWindow
{
    Q_OBJECT

public:
    static const QString DEFAULT_WALLET;

    explicit PIVXGUI(const NetworkStyle* networkStyle, QWidget* parent = 0);
    ~PIVXGUI();

    /** Set the client model.
        The client model represents the part of the core that communicates with the P2P network, and is wallet-agnostic.
    */
    void setClientModel(ClientModel* clientModel);

    //
    void goToDashboard();
    void goToSend();

#ifdef ENABLE_WALLET
    /** Set the wallet model.
        The wallet model represents a bitcoin wallet, and offers access to the list of transactions, address book and sending
        functionality.
    */
    bool addWallet(const QString& name, WalletModel* walletModel);
    bool setCurrentWallet(const QString& name);
    void removeAllWallets();


    void showHide(bool show){
        // TODO: Implement me.
    }
    // TODO: Change me..
    int getNavWidth(){
        return 100;
    }
#endif // ENABLE_WALLET

protected:
    /*
    void changeEvent(QEvent* e);
    void closeEvent(QCloseEvent* event);
    void dragEnterEvent(QDragEnterEvent* event);
    void dropEvent(QDropEvent* event);
    bool eventFilter(QObject* object, QEvent* event);
     */

private:
    bool enableWallet;
    ClientModel* clientModel;
    //WalletFrame* walletFrame;


    // Frame
    NavMenuWidget *navMenu;
    TopBar *topBar;
    QStackedWidget *stackedContainer;

    DashboardWidget *dashboard;
    SendWidget *sendWidget;


    //
    QSystemTrayIcon* trayIcon;
    Notificator* notificator;

    void createTrayIcon(const NetworkStyle* networkStyle);

signals:
    /** Signal raised when a URI was entered or dragged to the GUI */
    void receivedURI(const QString& uri);
    /** Restart handling */
    void requestedRestart(QStringList args);
};


#endif //PIVX_CORE_NEW_GUI_PIVXGUI_H
