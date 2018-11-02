// Copyright (c) 2011-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <core_io.h>
#include <utilstrencodings.h>

#include <qt/offlinetransactionsdialog.h>
#include <qt/forms/ui_offlinetransactionsdialog.h>
#include <qt/guiutil.h>

#include <qt/transactiontablemodel.h>

#include <QModelIndex>
#include <QClipboard>

#include <iostream>

/* XXX
std::string wrapString(const std::string input, int cols) {
    std::string out;
    size_t pos = 0;
    while (pos < input.size()) {
        out.append(input.substr(pos, cols));
        out.append("\n");
        pos += cols;
    }
    return out;
}
*/

/* XXX
The flow in this dialog is kind of fucked-up. Documenting it here for personal
reference before I document it more reasonably somewhere else:
- First you should use the send dialog in unsigned mode.
- In theory you should already at least have an unsigned offline transaction by
  the time you're touching this dialog. But if you don't, we should have a button
  you can push to go back to the send dialog and get one.
- If you come here from the send dialog, we want to go to tab one and display
  your transaction. This is expected to be on an online machine but it doesn't
  really matter -- if you get here, you get here.
- So the "whoops" pane of the first tab should probably be for use if you come
  not from the send dialog, and explain how to create an unsigned transaction,
  and maybe have a button to open the send dialog with the unsigned box checked.
  - this makes sense because there's no point in having an 'oops' pane of the first
    tab for offline-ness, actually.

    ...
- If you come here from
...
XXX */

/* XXX
- first tab:
  * normally get here from send dialog on the online computer
  - if we have a transaction, show it
  - otherwise, show an explanation of how to get one, and a suggestion to try other tabs
  - we should never get here unless we have a transaction
- second tab:
  * on the online computer, get here by pressing 'next' from the first tab
  * on the offline computer, get here from file->offlinestuff
  - if we have a transaction, show an explanation to use the offline computer
  - otherwise, if we're online, maybe show a warning?
  - otherwise, show a signing box
- third tab:
  * usually get here by pressing 'next' from the second tab on the online computer
  * if we close the dialog and need to get back to it, we have to get here by
      going file->offlinestuff on the online computer, UNLESS we can autodetect
      that we _had_ a transaction and closed the dialog, in which case do we go
      here or to tab 1? Conditioning on whether we clicked 'next' from tab 1
      is very complex.
  - if we're online, show a transaction broadcast box
*/

// can we just have separate 'sign' and 'broadcast' options in the menu?
//   we can probably do that if the offline stuff box is checked
//   maybe we add an 'offline' menu with all three steps, the first going to the
//   send box with offline prechecked

OfflineTransactionsDialog::OfflineTransactionsDialog(QWidget *parent, WalletModel *walletModel, ClientModel *clientModel) :
    QDialog(parent),
    ui(new Ui::OfflineTransactionsDialog),
    walletModel(walletModel),
    clientModel(clientModel)
{
    ui->setupUi(this);
    setWindowTitle("Review offline transaction");

    bool offline = false;
    onlineStateChanged(offline); // XXX base this on reality instead

    ui->transactionData1->setWordWrapMode(QTextOption::WrapAnywhere);  // XXX don't know how to set this property in Designer
    ui->transactionData2->setWordWrapMode(QTextOption::WrapAnywhere);
    ui->transactionData3->setWordWrapMode(QTextOption::WrapAnywhere);

    //XXX
    connect(ui->checkBoxOnlineOffline, SIGNAL(clicked(bool)), this, SLOT(onlineStateChanged(bool)));

    connect(ui->saveToFileButton1, &QPushButton::clicked, [this](){ saveToFile(ui->transactionData1); });
    connect(ui->saveToFileButton2, &QPushButton::clicked, [this](){ saveToFile(ui->transactionData2); });

    connect(ui->copyToClipboardButton1, &QPushButton::clicked, [this](){ clipboardCopy(ui->transactionData1); });
    connect(ui->copyToClipboardButton2, &QPushButton::clicked, [this](){ clipboardCopy(ui->transactionData2); });

    connect(ui->loadFromFileButton2, &QPushButton::clicked, [this]() { loadFromFile(ui->transactionData2); });
    connect(ui->loadFromFileButton3, &QPushButton::clicked, [this]() { loadFromFile(ui->transactionData3); });

    connect(ui->pasteButton2, &QPushButton::clicked, [this]() { clipboardPaste(ui->transactionData2); });
    connect(ui->pasteButton3, &QPushButton::clicked, [this]() { clipboardPaste(ui->transactionData3); });

    connect(ui->signTransactionButton, SIGNAL(clicked()), this, SLOT(signTransaction()));
    connect(ui->broadcastTransactionButton, SIGNAL(clicked()), this, SLOT(broadcastTransaction()));

    connect(ui->prevButton, SIGNAL(clicked()), this, SLOT(prevState()));
    connect(ui->nextButton, SIGNAL(clicked()), this, SLOT(nextState()));

    connect(ui->closeButton, SIGNAL(clicked()), this, SLOT(close()));

}

OfflineTransactionsDialog::~OfflineTransactionsDialog()
{
    delete ui;
}

void OfflineTransactionsDialog::setTransactionData(const std::string *transaction) {
    //XXX
    ui->transactionData1->setPlainText(QString::fromStdString(*transaction));
}

void OfflineTransactionsDialog::setWorkflowState(enum OfflineTransactionsDialog::WorkflowState state) {
    ui->nextButton->setEnabled(true);
    ui->prevButton->setEnabled(true);

    switch (state) {
    case OfflineTransactionsDialog::GetUnsignedTransaction:
        ui->prevButton->setEnabled(false);
        break;
    case OfflineTransactionsDialog::BroadcastTransaction:
        ui->nextButton->setEnabled(false);
        break;
    }

    // This shouldn't really be necessary, but seems to be?
    ui->nextButton->repaint();
    ui->prevButton->repaint();

    ui->tabWidget->setCurrentIndex(state);
}

enum OfflineTransactionsDialog::WorkflowState OfflineTransactionsDialog::workflowState() {
    return (OfflineTransactionsDialog::WorkflowState)
            ui->tabWidget->currentIndex();
}

void OfflineTransactionsDialog::nextState() {
    //XXX
    switch (workflowState()) {
    case OfflineTransactionsDialog::GetUnsignedTransaction:
        setWorkflowState(OfflineTransactionsDialog::SignTransaction);
        break;
    case OfflineTransactionsDialog::SignTransaction:
        setWorkflowState(OfflineTransactionsDialog::BroadcastTransaction);
        break;
    default:
        // Should never happen; nothing to do.
        break;
    }
}

void OfflineTransactionsDialog::prevState() {
    //XXX
    switch (workflowState()) {
    case OfflineTransactionsDialog::SignTransaction:
        setWorkflowState(OfflineTransactionsDialog::GetUnsignedTransaction);
        break;
    case OfflineTransactionsDialog::BroadcastTransaction:
        setWorkflowState(OfflineTransactionsDialog::SignTransaction);
        break;
    default:
        // Should never happen; nothing to do.
        break;
    }
}

void OfflineTransactionsDialog::saveToFile(const QPlainTextEdit *transactionData) {
    QString filename = GUIUtil::getSaveFileName(this,
        tr("Save Transaction Data"), QString(),
        tr("Partially Signed Transaction (*.psbt)"), nullptr);

    if (filename.isEmpty())
        return;

    std::ofstream out(filename.toLocal8Bit().data());
    out << transactionData->toPlainText().toStdString();
    out.close();
    // XXX binary instead?
}

void OfflineTransactionsDialog::loadFromFile(QPlainTextEdit *transactionData) {
    QString filename = GUIUtil::getOpenFileName(this,
        tr("Load Transaction Data"), QString(),
        tr("Partially Signed Transaction (*.psbt)"), nullptr);

    std::ifstream in(filename.toLocal8Bit().data());
    std::string data;
    in >> data;
    transactionData->setPlainText(QString::fromStdString(data));
}

void OfflineTransactionsDialog::clipboardCopy(const QPlainTextEdit *transactionData) {
    QApplication::clipboard()->setText(transactionData->toPlainText());
}

void OfflineTransactionsDialog::clipboardPaste(QPlainTextEdit *transactionData) {
    transactionData->setPlainText(QApplication::clipboard()->text());
}

void OfflineTransactionsDialog::onlineStateChanged(bool online) {
    ui->stackedWidgetStep2->setCurrentIndex(online); // only sign if we're offline
    ui->stackedWidgetStep3->setCurrentIndex(online); // only broadcast if we're online
}

/* XXX
        // Create unsigned transaction, don't send
        PartiallySignedTransaction psbtx(currentTransaction.getWtx()->get());
        //... XXX should we do this inside OTD maybe???
        model->FillPSBT(psbtx, 1 / * XXX constant -- SIGHASH_ALL? * /, false, true);

        // Serialize the PSBT
        CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
        ssTx << psbtx;
        std::string result = EncodeBase64((unsigned char*)ssTx.data(), ssTx.size());

        OfflineTransactionsDialog *dlg = new OfflineTransactionsDialog(this, model, clientModel);
        dlg->setTransactionData(&result);
        dlg->setWorkflowState(OfflineTransactionsDialog::GetUnsignedTransaction);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->exec();
        // XXX END ganked from rpcwallet.cpp walletcreatefundedpsbt
*/

void OfflineTransactionsDialog::signTransaction() {
    //XXX -- just call fillpsbt again with sign set to true
    PartiallySignedTransaction psbt;
    std::string error;
    if (!DecodePSBT(psbt, ui->transactionData2->toPlainText().toStdString(), error)) {
        // XXX miserable failure, signal some kind of error here
        return;
    }

    if (!walletModel->FillPSBT(psbt, SIGHASH_ALL, true, true)) {
        //XXX oops, it worked but warn that it's still incomplete and can't be broadcast yet
        // maybe colorize or something?
    }

    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << psbt;
    ui->transactionData2->setPlainText(QString::fromStdString(EncodeBase64(ssTx.str())));
}

void OfflineTransactionsDialog::broadcastTransaction() {
    PartiallySignedTransaction psbtx;
    std::string error;
    if (!DecodePSBT(psbtx, ui->transactionData3->toPlainText().toStdString(), error)) {
        // XXX miserable failure, signal some kind of error here
        return;
    }

    // XXX hmm, the psbt code that's not wallet-based should NOT go through teh wallet model. But somehow we should be able to call out to it instead of open-code it.
    bool complete;
    complete = true;
    for (unsigned int i = 0; i < psbtx.tx->vin.size(); ++i) {
        complete &= SignPSBTInput(DUMMY_SIGNING_PROVIDER, psbtx, i, SIGHASH_ALL);
    }
    if (!complete) {
        // XXX miserable failure, signal error
        return;
    }
    CMutableTransaction mtx(*psbtx.tx);
    for (unsigned int i = 0; i < mtx.vin.size(); ++i) {
        mtx.vin[i].scriptSig = psbtx.inputs[i].final_script_sig;
        mtx.vin[i].scriptWitness = psbtx.inputs[i].final_script_witness;
    }
    CTransactionRef tx = MakeTransactionRef(mtx);
    std::string txid = walletModel->BroadcastTransaction(tx);
    ui->transactionData3->setPlainText(QString::fromStdString(txid + " is the txid that we broadcast."));
}
