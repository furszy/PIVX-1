// Copyright (c) 2021 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef VOTEDIALOG_H
#define VOTEDIALOG_H

#include <QDialog>
#include <QCheckBox>
#include <QProgressBar>

namespace Ui {
class VoteDialog;
}

class VoteDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VoteDialog(QWidget *parent = nullptr);
    ~VoteDialog();

    void showEvent(QShowEvent *event) override;

public Q_SLOTS:
    void onAcceptClicked();
    void onCheckBoxClicked(QCheckBox* checkBox, QProgressBar* progressBar, bool isVoteYes);

private:
    Ui::VoteDialog *ui;
    QCheckBox* checkBoxNo{nullptr};
    QCheckBox* checkBoxYes{nullptr};
    QProgressBar* progressBarNo{nullptr};
    QProgressBar* progressBarYes{nullptr};

    void initVoteCheck(QWidget* container, QCheckBox* checkBox, QProgressBar* progressBar,
                       QString text, Qt::LayoutDirection direction, bool isVoteYes);
};

#endif // VOTEDIALOG_H
