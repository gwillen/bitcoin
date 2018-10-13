// Copyright (c) 2011-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <qt/asdftestdialog.h>
#include <qt/forms/ui_asdftestdialog.h>

#include <qt/guiconstants.h>
#include <qt/walletmodel.h>

#include <support/allocators/secure.h>

#include <QKeyEvent>
#include <QMessageBox>
#include <QPushButton>

AsdfTestDialog::AsdfTestDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AsdfTestDialog)
{
    ui->setupUi(this);
}

AsdfTestDialog::~AsdfTestDialog()
{
    delete ui;
}
