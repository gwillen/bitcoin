// Copyright (c) 2011-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/offlinetransactionsdialog.h>

#include <core_io.h>
#include <interfaces/node.h>
#include <key_io.h>
#include <qt/bitcoinunits.h>
#include <qt/forms/ui_offlinetransactionsdialog.h>
#include <qt/guiutil.h>
#include <qt/transactiontablemodel.h>
#include <util/strencodings.h>

#include <QModelIndex>
#include <QClipboard>

#include <iostream>

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

//XXX
std::string serializeTransaction(PartiallySignedTransaction psbtx) {
    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << psbtx;
    return ssTx.str();
}

OfflineTransactionsDialog::OfflineTransactionsDialog(QWidget *parent, WalletModel *walletModel, ClientModel *clientModel) :
    QDialog(parent),
    ui(new Ui::OfflineTransactionsDialog),
    walletModel(walletModel),
    clientModel(clientModel)
{
    ui->setupUi(this);
    setWindowTitle("Review offline transaction");

    ui->stackedWidgetStep2->setCurrentIndex(1); //XXX // only sign if we're offline
    ui->stackedWidgetStep3->setCurrentIndex(0); //XXX only broadcast if we're online

    transactionText[1] = ui->transactionData1;
    transactionText[2] = ui->transactionData2;
    transactionText[3] = ui->transactionData3;

    for (int i = 1; i <= 3; ++i) {
        transactionText[i]->setWordWrapMode(QTextOption::WrapAnywhere);
    }

    connect(ui->saveToFileButton1, &QPushButton::clicked, [this](){ saveToFile(1); });
    connect(ui->saveToFileButton2, &QPushButton::clicked, [this](){ saveToFile(2); });

    connect(ui->copyToClipboardButton1, &QPushButton::clicked, [this](){ clipboardCopy(1); });
    connect(ui->copyToClipboardButton2, &QPushButton::clicked, [this](){ clipboardCopy(2); });

    connect(ui->loadFromFileButton2, &QPushButton::clicked, [this]() { loadFromFile(2); });
    connect(ui->loadFromFileButton3, &QPushButton::clicked, [this]() { loadFromFile(3); });

    connect(ui->pasteButton2, &QPushButton::clicked, [this]() { clipboardPaste(2); });
    connect(ui->pasteButton3, &QPushButton::clicked, [this]() { clipboardPaste(3); });

    connect(ui->signTransactionButton, &QPushButton::clicked, this, &OfflineTransactionsDialog::signTransaction);
    connect(ui->broadcastTransactionButton, &QPushButton::clicked, this, &OfflineTransactionsDialog::broadcastTransaction);

    connect(ui->resetButton3, &QPushButton::clicked, this, &OfflineTransactionsDialog::resetAssembledTransaction);
    connect(ui->closeButton, &QPushButton::clicked, this, &OfflineTransactionsDialog::close);
    connect(ui->checkBoxAdvanced, &QCheckBox::clicked, this, &OfflineTransactionsDialog::advancedClicked);

    // Initialize advanced buttons to be hidden
    advancedClicked(false);
}

OfflineTransactionsDialog::~OfflineTransactionsDialog()
{
    delete ui;
}

void OfflineTransactionsDialog::advancedClicked(bool checked) {
    ui->copyToClipboardButton1->setVisible(checked);
    ui->copyToClipboardButton2->setVisible(checked);
    ui->pasteButton2->setVisible(checked);
    ui->pasteButton3->setVisible(checked);

    for (int tabId = 1; tabId <= 3; ++tabId) {
        if (transactionData[tabId].tx) {  // XXX should be outer layer optional
            transactionText[tabId]->setPlainText(QString::fromStdString(renderTransaction(transactionData[tabId])));
        }
    }
}

void OfflineTransactionsDialog::setFirstTabTransaction(const CTransactionRef tx) {
    setWorkflowState(OfflineTransactionsDialog::GetUnsignedTransaction);

    PartiallySignedTransaction psbtx((CMutableTransaction)(*tx));
    bool complete;
    // XXX hm, this is a gross place to do all this work though.
    TransactionError err = walletModel->FillPSBT(psbtx, complete, SIGHASH_ALL, false, true);
    if (err != TransactionError::OK) {
        // XXX do something with err
        return;
    }
    if (!complete) {
        // XXX warn it's not complete?
    }

    transactionData[1] = psbtx;
    ui->transactionData1->setPlainText(QString::fromStdString(renderTransaction(transactionData[1])));
}

//XXX is this still necessary with prev/next buttons gone
void OfflineTransactionsDialog::setWorkflowState(enum OfflineTransactionsDialog::WorkflowState state) {
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

void OfflineTransactionsDialog::saveToFile(int tabId) {
    QString filename = GUIUtil::getSaveFileName(this,
        tr("Save Transaction Data"), QString(),
        tr("Partially Signed Transaction (*.psbt)"), nullptr);
    if (filename.isEmpty()) {
        return;
    }

    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << transactionData[tabId];
    std::ofstream out(filename.toLocal8Bit().data());
    out << ssTx.str(); // XXX should we allow writing base64?
    out.close();
}

void OfflineTransactionsDialog::loadTransaction(int tabId, std::string data) {
    std::string error;
    PartiallySignedTransaction psbtx;
    if (!DecodeRawPSBT(psbtx, data, error)) {
        // XXX this is bad, signal "error"
        return;
    }

    if (tabId == 3 && started_tx_assembly) {
        if (!transactionData[tabId].Merge(psbtx)) {
            // XXX this is bad, it wasn't the same transaction, signal error
        }
    } else {
        transactionData[tabId] = psbtx;
    }

    if (tabId == 3) {
        started_tx_assembly = true;
        ui->loadFromFileButton3->setText("Load more ...");
        PSBTAnalysis analysis = AnalyzePSBT(transactionData[tabId]);
        bool have_all_sigs = (analysis.next == PSBTRole::FINALIZER) || (analysis.next == PSBTRole::EXTRACTOR);
        ui->broadcastTransactionButton->setEnabled(have_all_sigs);
    }

    if (tabId == 2) {
        // XXX, this also needs to happen if we clear the tab some other way?
        did_sign_tx = false;
    }

    // XXX re-encoding here could be slightly rude and mask issues if it's not bijective... is it?
    transactionText[tabId]->setPlainText(QString::fromStdString(renderTransaction(transactionData[tabId])));
}

void OfflineTransactionsDialog::loadFromFile(int tabId) {
    QString filename = GUIUtil::getOpenFileName(this,
        tr("Load Transaction Data"), QString(),
        tr("Partially Signed Transaction (*.psbt)"), nullptr);
    if (filename.isEmpty()) {
        return;
    }

    std::ifstream in(filename.toLocal8Bit().data(), std::ios::binary);
    // https://stackoverflow.com/questions/116038/what-is-the-best-way-to-read-an-entire-file-into-a-stdstring-in-c
    std::string data(std::istreambuf_iterator<char>{in}, {});

    loadTransaction(tabId, data);
}

void OfflineTransactionsDialog::clipboardCopy(int tabId) {
    QApplication::clipboard()->setText(QString::fromStdString(EncodeBase64(serializeTransaction(transactionData[tabId]))));
}

void OfflineTransactionsDialog::clipboardPaste(int tabId) {
    std::string data = QApplication::clipboard()->text().toStdString();
    bool invalid;
    std::string decoded = DecodeBase64(data, &invalid);
    if (invalid) {
        // XXX this is bad
        return;
    }

    loadTransaction(tabId, decoded);
}

void OfflineTransactionsDialog::signTransaction() {
    bool complete;
    TransactionError err = walletModel->FillPSBT(transactionData[2], complete, SIGHASH_ALL, true, true);
    if (err != TransactionError::OK) {
        // XXX this is a failure, do something with err
        return;
    }

    if (!complete) {
        //XXX oops, it worked but warn that it's still incomplete and can't be broadcast yet
        // maybe colorize or something?
    }

    did_sign_tx = true;
    ui->transactionData2->setPlainText(QString::fromStdString(renderTransaction(transactionData[2])));
}

void OfflineTransactionsDialog::broadcastTransaction() {
    // XXX these need to be option<> -- if we try to broadcast with no transaction we crash here. (and probably need more try{} blocks...)
    PartiallySignedTransaction psbtx = transactionData[3];
    // XXX hmm, the psbt code that's not wallet-based should NOT go through teh wallet model. But somehow we should be able to call out to it instead of open-code it.
    bool complete = true;
    for (unsigned int i = 0; i < psbtx.tx->vin.size(); ++i) {
        // We're not just broadcasting here, we're finalizing, which is slightly misleading?
        complete &= SignPSBTInput(DUMMY_SIGNING_PROVIDER, psbtx, i, SIGHASH_ALL);
    }
    if (!complete) {
        // XXX miserable failure, signal error
        // XXX we mutated our transaction here, should probably have updated the display...
        return;
    }
    CMutableTransaction mtx(*psbtx.tx);
    for (unsigned int i = 0; i < mtx.vin.size(); ++i) {
        mtx.vin[i].scriptSig = psbtx.inputs[i].final_script_sig;
        mtx.vin[i].scriptWitness = psbtx.inputs[i].final_script_witness;
    }
    CTransactionRef tx = MakeTransactionRef(mtx);
    std::string message;
    uint256 txid;
    std::string err_string;
    TransactionError error = clientModel->node().broadcastTransaction(tx, txid, err_string);
    if (error == TransactionError::OK) {
        message = "Transaction broadcast successfully! Transaction ID: " + txid.GetHex();
    } else {
        message = "Transaction broadcast failed: "+ err_string;
    }

    resetAssembledTransaction();
    ui->transactionData3->setPlainText(QString::fromStdString(message)); // XXX this is gross
}

void OfflineTransactionsDialog::resetAssembledTransaction() {
    started_tx_assembly = false;
    transactionData[3] = PartiallySignedTransaction();
    transactionText[3]->setPlainText("");
    ui->loadFromFileButton3->setText("Load from file ...");
    ui->broadcastTransactionButton->setEnabled(false);
}

std::string OfflineTransactionsDialog::renderTransaction(PartiallySignedTransaction psbtx) {
    PSBTAnalysis analysis = AnalyzePSBT(psbtx);

    // XXX copied from sendcoinsdialog.cpp
    QString questionString = "";

    if (did_sign_tx) {
        // XXX except, we didn't check whether our signing actually did anything, or whether there was even anything we could sign.
        questionString.append("SIGNED!\n\n");
        // XXX also, this is bad because it puts SIGNED in the broadcast box when we paste a transaction after signing -- in general 'signed' is not a state that sticks around, or is cross-tab -- we should have this as something like a butterbar on tab 2, instead.
    }

    questionString.append("Transaction preview:\n");  // XXX removed tr() macro

    CAmount txFee = 0;
    if (analysis.fee) {
        txFee = *analysis.fee;
    }

    if (analysis.next == PSBTRole::FINALIZER || analysis.next == PSBTRole::EXTRACTOR) {
        questionString.append("Transaction is fully signed and ready for broadcast.\n");
    } else {
        questionString.append("Transaction still needs signature(s).\n");
    }

    for (const CTxOut& out : psbtx.tx->vout) {
        CTxDestination address;
        ExtractDestination(out.scriptPubKey, address);
        questionString.append(QString("Sends %1 bitcoins to %2\n").arg((double)out.nValue / (double)COIN).arg(QString::fromStdString(EncodeDestination(address))));
    }

    if(txFee > 0)
    {
        // append fee string if a fee is required
        questionString.append(tr("Transaction fee: "));

        // append transaction size
        //questionString.append(" (" + QString::number((double)currentTransaction.getTransactionSize() / 1000) + " kB): ");

        // append transaction fee value
        questionString.append(BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, txFee));
        //XXXquestionString.append(txFee);

        // append RBF message according to transaction's signalling
        //questionString.append("<span style='font-size:10pt; font-weight:normal;'>");
        //if (ui->optInRBF->isChecked()) {
        //    questionString.append(tr("You can increase the fee later (signals Replace-By-Fee, BIP-125)."));
        //} else {
        //    questionString.append(tr("Not signalling Replace-By-Fee, BIP-125."));
        //}
        //questionString.append("</span>");
    }
/*
    // add total amount in all subdivision units
    questionString.append("<hr />");
    CAmount totalAmount = currentTransaction.getTotalTransactionAmount() + txFee;
    QStringList alternativeUnits;
    for (const BitcoinUnits::Unit u : BitcoinUnits::availableUnits())
    {
        if(u != model->getOptionsModel()->getDisplayUnit())
            alternativeUnits.append(BitcoinUnits::formatHtmlWithUnit(u, totalAmount));
    }
    questionString.append(QString("<b>%1</b>: <b>%2</b>").arg(tr("Total Amount"))
        .arg(BitcoinUnits::formatHtmlWithUnit(model->getOptionsModel()->getDisplayUnit(), totalAmount)));
    questionString.append(QString("<br /><span style='font-size:10pt; font-weight:normal;'>(=%1)</span>")
        .arg(alternativeUnits.join(" " + tr("or") + " ")));

    SendConfirmationDialog confirmationDialog(tr("Confirm send coins"),
        questionString.arg(formatted.join("<br />")), SEND_CONFIRM_DELAY, this);
    }
    */
    if (ui->checkBoxAdvanced->isChecked()) {
        questionString.append("\n\n");
        questionString.append(QString::fromStdString(EncodeBase64(serializeTransaction(psbtx))));
        questionString.append("\n\n");
        questionString.append("Debug info: (XXX disabled) ");
        //XXX questionString.append(analysis.write().c_str());
    }
    return questionString.toStdString();
}

//XXX

bool GetUTXO(PSBTInput& pi, CTxOut& utxo, int prevout_index)
{
    if (pi.non_witness_utxo) {
        utxo = pi.non_witness_utxo->vout[prevout_index];
    } else if (!pi.witness_utxo.IsNull()) {
        utxo = pi.witness_utxo;
    } else {
        return false;
    }
    return true;
}
