// Copyright (c) 2011-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_ASDFTESTDIALOG_H
#define BITCOIN_QT_ASDFTESTDIALOG_H

#include <QDialog>

namespace Ui {
    class AsdfTestDialog;
}

/** Dialog serving no purpose.
 */
class AsdfTestDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AsdfTestDialog(QWidget *parent);
    ~AsdfTestDialog();

private:
    Ui::AsdfTestDialog *ui;
};

#endif // BITCOIN_QT_AsdfTestDialog_H
