// Copyright (c) 2011-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_OFFLINETRANSACTIONSDIALOG_H
#define BITCOIN_QT_OFFLINETRANSACTIONSDIALOG_H

#include <QDialog>
#include <QPlainTextEdit>

#include <qt/clientmodel.h>
#include <qt/walletmodel.h>

namespace Ui {
    class OfflineTransactionsDialog;
}

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Dialog showing transaction details. */
class OfflineTransactionsDialog : public QDialog
{
    Q_OBJECT

public:
    // Correspond 1:1 to tab IDs
    enum WorkflowState {
        GetUnsignedTransaction = 0,
        SignTransaction = 1,
        BroadcastTransaction = 2,
    };

    explicit OfflineTransactionsDialog(QWidget *parent, WalletModel *walletModel, ClientModel *clientModel);
    ~OfflineTransactionsDialog();

    void setTransactionData(const std::string *transaction);
    void setWorkflowState(enum WorkflowState);
    enum WorkflowState workflowState();

public Q_SLOTS:
    void saveToFile(const QPlainTextEdit *transactionData);
    void loadFromFile(QPlainTextEdit *transactionData);
    void clipboardCopy(const QPlainTextEdit *transactionData);
    void clipboardPaste(QPlainTextEdit *transactionData);

    void nextState();
    void prevState();

    void signTransaction();
    void broadcastTransaction();

    void onlineStateChanged(bool online);

private:
    Ui::OfflineTransactionsDialog *ui;
    QPlainTextEdit transactionData[3];  // indexed by tab
    WalletModel *walletModel;
    ClientModel *clientModel;
};

#endif // BITCOIN_QT_OFFLINETRANSACTIONSDIALOG_H
