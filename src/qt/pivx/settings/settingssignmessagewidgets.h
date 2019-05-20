#ifndef SETTINGSSIGNMESSAGEWIDGETS_H
#define SETTINGSSIGNMESSAGEWIDGETS_H

#include <QWidget>
#include "qt/pivx/pwidget.h"
#include "qt/pivx/contactsdropdown.h"

namespace Ui {
class SettingsSignMessageWidgets;
}

class SettingsSignMessageWidgets : public PWidget
{
    Q_OBJECT

public:
    explicit SettingsSignMessageWidgets(PIVXGUI* _window, QWidget *parent = nullptr);
    ~SettingsSignMessageWidgets();

    void setAddress_SM(const QString& address);
protected:
    void resizeEvent(QResizeEvent *event) override;
public slots:
    void on_signMessageButton_SM_clicked();
    void on_pasteButton_SM_clicked();
    void on_addressBookButton_SM_clicked();
    void on_clear_all();
    void onAddressesClicked();
private:
    Ui::SettingsSignMessageWidgets *ui;
    QAction *btnContact;
    ContactsDropdown *menuContacts = nullptr;
    void resizeMenu();
};

#endif // SETTINGSSIGNMESSAGEWIDGETS_H
