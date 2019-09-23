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

    explicit OfflineTransactionsDialog(QWidget* parent, WalletModel* walletModel, ClientModel* clientModel);
    ~OfflineTransactionsDialog();

    void openWithTransaction(const CTransactionRef tx);
    void setWorkflowState(enum WorkflowState);

public Q_SLOTS:
    void saveToFile(int tabId);
    void loadFromFile(int tabId);
    void clipboardCopy(int tabId);
    void clipboardPaste(int tabId);

    void advancedClicked(bool value);

    void signTransaction();
    void broadcastTransaction();

    void resetAssembledTransaction();

private:
    Ui::OfflineTransactionsDialog* ui;
    Optional<PartiallySignedTransaction> transactionData[4]; // 1-indexed by tab to avoid confusion; 0 unused
    bool started_tx_assembly = false;
    QPlainTextEdit*(transactionText[4]); // 1-indexed by tab to avoid confusion; 0 unused
    WalletModel* walletModel;
    ClientModel* clientModel;

    std::string renderTransaction(Optional<PartiallySignedTransaction> psbtx);
    void loadTransaction(int tabId, std::string data);
    void setTransaction(int tabId, const Optional<PartiallySignedTransaction> &psbt);
    bool showMessageBox(const std::string& message, const std::string& caption, unsigned int style);
};

#endif // BITCOIN_QT_OFFLINETRANSACTIONSDIALOG_H
