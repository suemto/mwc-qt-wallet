// Copyright 2019 The MWC Developers
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QTime>
#include <QDebug>
#include <QContextMenuEvent>
//#include "../state/statemachine.h"
//#include "util/stringutils.h"
#include "../control_desktop/messagebox.h"
#include "../dialogs_desktop/helpdlg.h"
#include "../bridge/config_b.h"
#include "../bridge/corewindow_b.h"
#include "../bridge/wallet_b.h"
#include "../bridge/statemachine_b.h"
#include "../bridge/util_b.h"
#include <QPushButton>
#include <QApplication>
#include <QDesktopWidget>
#include <QScrollArea>
#include <QStyle>
#include "../core/global.h"
#include "../core/WalletApp.h"
#include "../core_desktop/statuswndmgr.h"

namespace core {

using namespace bridge;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    config = new bridge::Config(this);
    coreWindow = new bridge::CoreWindow(this);
    wallet = new bridge::Wallet(this);
    stateMachine = new bridge::StateMachine(this);
    util = new bridge::Util(this);

    statusMgr = new core::StatusWndMgr(this);

    QObject::connect( coreWindow, &CoreWindow::sgnUpdateActionStates,
                      this, &MainWindow::onSgnUpdateActionStates, Qt::QueuedConnection);

    QObject::connect( wallet, &Wallet::sgnNewNotificationMessage,
                      this, &MainWindow::onSgnNewNotificationMessage, Qt::QueuedConnection);
    QObject::connect(wallet, &Wallet::sgnConfigUpdate,
                     this, &MainWindow::onSgnConfigUpdate, Qt::QueuedConnection);
    QObject::connect(wallet, &Wallet::sgnLoginResult,
                     this, &MainWindow::onSgnLoginResult, Qt::QueuedConnection);

    QObject::connect(wallet, &Wallet::sgnUpdateListenerStatus,
                     this, &MainWindow::onSgnUpdateListenerStatus, Qt::QueuedConnection);
    QObject::connect(wallet, &Wallet::sgnHttpListeningStatus,
                     this, &MainWindow::onSgnHttpListeningStatus, Qt::QueuedConnection);

    QObject::connect(wallet, &Wallet::sgnUpdateNodeStatus,
                     this, &MainWindow::onSgnUpdateNodeStatus, Qt::QueuedConnection);

    QObject::connect(wallet, &Wallet::sgnUpdateSyncProgress,
                     this, &MainWindow::onSgnUpdateSyncProgress, Qt::QueuedConnection);

    updateListenerBtn();
    updateNetworkName();
    updateMenu();


    ui->leftTb->hide();

    ui->statusBar->addPermanentWidget(ui->nodeStatusButton);
    ui->statusBar->addPermanentWidget(ui->listenerStatusButton);
    ui->statusBar->addPermanentWidget(ui->btnSpacerLabel1);
    ui->statusBar->addPermanentWidget(ui->helpButton);
    ui->statusBar->addPermanentWidget(ui->rightestSpacerLabel);

    if (config->isOnlineNode()) {
        ui->helpButton->hide();
        ui->listenerStatusButton->hide();
        ui->btnSpacerLabel1->hide();
    }

    if (config->isColdWallet()) {
        ui->listenerStatusButton->hide();
    }

    setStatusButtonState(ui->nodeStatusButton, STATUS::GREEN, "Waiting");
    setStatusButtonState(ui->listenerStatusButton, STATUS::RED, "Listeners");

    //ui->statusBar->showMessage("Can show any message here", 2000);

    // Want to delete children when they close
    setAttribute( Qt::WA_DeleteOnClose, true );

    // Update size by max screen size
    QSize wndSize = frameSize();
    QSize newSize = wndSize;
    QRect screenRc = QDesktopWidget().availableGeometry(this);

    const int DX = 10;
    const int DY = 30;

    if (newSize.width() > screenRc.width() - DX )
        newSize.setWidth( screenRc.width() - DX );

    if (newSize.height() > screenRc.height() - DY )
        newSize.setHeight( screenRc.height() - DY );

    if ( newSize != wndSize ) {
        QSize newSz = size() - ( wndSize - newSize );

        setGeometry( (screenRc.width() - newSize.width())/2,
                     (screenRc.height() - newSize.height())/2,
                     newSz.width(), newSz.height() );
    }
}

void MainWindow::repost() {
    int id = mwc::getRepostId();
    QString account = mwc::getRepostAccount();
    wallet->repost(account, id, false);
}

void MainWindow::repost_as_fluff() {
    int id = mwc::getRepostId();
    QString account = mwc::getRepostAccount();
    wallet->repost(account, id, true);
}


#ifndef QT_NO_CONTEXTMENU
void MainWindow::contextMenuEvent(QContextMenuEvent *event)
{
    if(mwc::getRepostId() >= 0) {
        QMenu contextMenu(tr("Context menu"), this);
        QAction action1("Repost", coreWindow);
        QAction action2("Repost as a fluff transaction", coreWindow);

        QObject::connect( &action1, SIGNAL(triggered()),
                      SLOT(repost()), Qt::QueuedConnection);
        QObject::connect( &action2, SIGNAL(triggered()),
                      SLOT(repost_as_fluff()), Qt::QueuedConnection);

        contextMenu.addAction(&action1);
        contextMenu.addAction(&action2);

        contextMenu.exec(event->globalPos());
    }
}
#endif /* QT_NO_CONTEXTMENU */

MainWindow::~MainWindow()
{
    delete ui;
    delete statusMgr;
}

void MainWindow::restore() {
    statusMgr->restore();
}

void MainWindow::statusHide(const QSharedPointer<StatusWnd> _swnd) {
    statusMgr->statusHide(_swnd);
}

void MainWindow::statusDone(const QSharedPointer<StatusWnd> _swnd) {
    statusMgr->statusDone(_swnd);
}

// level: notify::MESSAGE_LEVEL
void MainWindow::onSgnNewNotificationMessage(int level, QString message) {

    using namespace wallet;

    QString prefix;
    int timeout = 3000;
    switch( notify::MESSAGE_LEVEL(level)) {
        case notify::MESSAGE_LEVEL::FATAL_ERROR:
            prefix = "Error: ";
            timeout = 7000;
            break;
        case notify::MESSAGE_LEVEL::CRITICAL:
            prefix = "Critical: ";
            timeout = 7000;
            break;
        case notify::MESSAGE_LEVEL::WARNING:
            prefix = "Warning: ";
            timeout = 7000;
            break;
        case notify::MESSAGE_LEVEL::INFO:
            prefix = "Info: ";
            timeout = 4000;
            break;
        case notify::MESSAGE_LEVEL::DEBUG:
            prefix = "Debug: ";
            timeout = 2000;
            break;
    }

    statusMgr->handleStatusMessage(prefix, message);
    ui->statusBar->showMessage( prefix + message, (int)(timeout * config->getTimeoutMultiplier()) );
}


void MainWindow::updateLeftBar(bool show) {
    if (leftBarShown == show)
        return;

    if (show) {
        ui->leftTb->show();
        ui->statusBar->show();
    }
    else {
        ui->leftTb->hide();
        ui->statusBar->hide();
    }
    leftBarShown = show;
}

QWidget * MainWindow::getMainWindow() {
    return ui->mainWindowFrame;
}

// actionState:  state::STATE
void MainWindow::onSgnUpdateActionStates(int actionState) {
    if (core::WalletApp::isExiting())
        return;

    bool isLeftBarVisible = (actionState >= state::STATE::ACCOUNTS && actionState != state::STATE::RESYNC );

    updateLeftBar( isLeftBarVisible );

    updateMenu();
}

void core::MainWindow::on_listenerStatusButton_clicked()
{
    stateMachine->setActionWindow( state::STATE::LISTENING);
}

void core::MainWindow::on_nodeStatusButton_clicked()
{
    stateMachine->setActionWindow( state::STATE::NODE_INFO );
}

void MainWindow::on_helpButton_clicked()
{
    QString docName = stateMachine->getCurrentHelpDocName();
    dlg::HelpDlg helpDlg(this, docName);
    helpDlg.exec();
}

void MainWindow::onSgnUpdateListenerStatus(bool mwcOnline, bool keybaseOnline, bool tor) {
    Q_UNUSED(mwcOnline)
    Q_UNUSED(keybaseOnline)
    Q_UNUSED(tor);
    updateListenerBtn();
}

void MainWindow::onSgnHttpListeningStatus(bool listening, QString additionalInfo) {
    Q_UNUSED(listening)
    Q_UNUSED(additionalInfo)
    updateListenerBtn();
}

// Node info
void MainWindow::onSgnUpdateNodeStatus( bool online, QString errMsg, int nodeHeight, int peerHeight, int64_t totalDifficulty, int connections ) {
    Q_UNUSED(errMsg)
    Q_UNUSED(totalDifficulty)
    if ( !online ) {
        setStatusButtonState( ui->nodeStatusButton, STATUS::RED, "" );
    }
    else if (connections==0 || nodeHeight==0 || (peerHeight>0 && peerHeight-nodeHeight>5) ) {
        setStatusButtonState( ui->nodeStatusButton, STATUS::YELLOW, "" );
    }
    else {
        setStatusButtonState( ui->nodeStatusButton, STATUS::GREEN, "" );
    }
}

void MainWindow::onSgnUpdateSyncProgress(double progressPercent) {
    onSgnNewNotificationMessage( int(notify::MESSAGE_LEVEL::INFO),
                             "Wallet state update, " + util::trimStrAsDouble( QString::number(progressPercent), 4 ) + "% complete"  );
}

void MainWindow::onSgnConfigUpdate() {
    updateNetworkName();
}

void MainWindow::onSgnLoginResult(bool ok) {
    Q_UNUSED(ok)
    updateNetworkName();
}


void MainWindow::updateListenerBtn() {
    bool mqsStatus = wallet->getMqsListenerStatus();
    bool keybaseStatus = wallet->getKeybaseListenerStatus();
    bool torStatus = wallet->getTorListenerStatus();
    QString httpListenerStatus = wallet->getHttpListeningStatus();

    qDebug() << "updateListenerBtn: mqsStatus=" << mqsStatus << " keybaseStatus=" << keybaseStatus << " torStatus=" << torStatus << " httpListenerStatus=" << httpListenerStatus;

    bool listening = mqsStatus | keybaseStatus | torStatus;
    QString listenerNames;
    if (mqsStatus)
        listenerNames +=  QString("MWC MQS");

    if (keybaseStatus) {
        if (!listenerNames.isEmpty())
            listenerNames += ", ";
        listenerNames += "Keybase";
    }

    if (torStatus) {
        if (!listenerNames.isEmpty())
            listenerNames += ", ";
        listenerNames += "TOR";
    }

    if (httpListenerStatus == "true") {
        listening = true;
        if (!listenerNames.isEmpty())
            listenerNames += ", ";
        listenerNames += "Http";
        if (config->hasTls())
            listenerNames += "s";
    }

    setStatusButtonState( ui->listenerStatusButton,
                          listening ? STATUS::GREEN : STATUS::RED,
                          listening ? listenerNames : "Listeners");
    ui->listenerStatusButton->setToolTip(listening ? "You are listening. Click here to view listener status"
                                          : "You are not listening. Click here to view listener status");
}


void MainWindow::updateNetworkName() {
    setStatusButtonState( ui->nodeStatusButton, STATUS::IGNORE, config->getNetwork() );
}

void MainWindow::setStatusButtonState(  QPushButton * btn, STATUS status, QString text ) {
    switch (status) {
        case STATUS::GREEN:
            btn->setIcon( QIcon( QPixmap( ":/img/CircGreen@2x.svg" )));
            break;
        case STATUS::RED:
            btn->setIcon( QIcon( QPixmap( ":/img/CircRed@2x.svg" )));
            break;
        case STATUS::YELLOW:
            btn->setIcon( QIcon( QPixmap( ":/img/CircYellow@2x.svg" )));
            break;
        default: // Ingnore suppose to be here
            break;
    }

    if (!text.isEmpty())
        btn->setText(" " + text + " ");
}

void MainWindow::moveEvent(QMoveEvent* event) {
    // yes, it can be possible
    if (statusMgr!=nullptr)
        statusMgr->moveEvent(event);
    QMainWindow::moveEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    // yes, it can be possible
    if (statusMgr!=nullptr)
        statusMgr->resizeEvent(event);

    QMainWindow::resizeEvent(event);
}

bool MainWindow::event(QEvent* event) {
    // yes, it can be possible
    if (statusMgr!=nullptr) {
        if (statusMgr->event(event)) {
            return true;
        }
    }
    return QMainWindow::event(event);
}

//////////////////////////////////////////////////////////////////////////////////
///   Menu commands...
////

void MainWindow::updateMenu() {
    if (stateMachine==nullptr)
        return;

    bool canSwitchState = stateMachine->canSwitchState(state::STATE::NONE) && stateMachine->getCurrentStateId() >= state::STATE::ACCOUNTS;
    bool isOnlineWallet = config->isOnlineWallet();
    bool isColdWallet = config->isColdWallet();
    bool isOnlineNode = config->isOnlineNode();

    ui->actionSend->setEnabled(canSwitchState && !isOnlineNode);
    ui->actionReceive->setEnabled(canSwitchState && !isOnlineNode);
    ui->actionFinalize->setEnabled(canSwitchState && !isOnlineNode);
    ui->actionTransactions->setEnabled(canSwitchState && !isOnlineNode);
    ui->actionListeners->setEnabled(canSwitchState && !isColdWallet && !isOnlineNode);
    ui->actionNode_Overview->setEnabled(canSwitchState);
    ui->actionResync_with_full_node->setEnabled(canSwitchState && !isOnlineNode);
    ui->actionOutputs->setEnabled(canSwitchState && !isOnlineNode);
    ui->actionAirdrop->setEnabled(canSwitchState && isOnlineWallet);
    ui->actionHODL->setEnabled(canSwitchState && !isColdWallet);
    ui->actionWallet_accounts->setEnabled(canSwitchState && !isOnlineNode);
    ui->actionAccounts->setEnabled(canSwitchState && !isOnlineNode);
    ui->actionContacts->setEnabled(canSwitchState && !isOnlineNode);
    ui->actionShow_passphrase->setEnabled(canSwitchState && !isOnlineNode);
    ui->actionEvent_log->setEnabled(canSwitchState);
    ui->actionLogout->setEnabled(canSwitchState && !isOnlineNode);
    ui->actionConfig->setEnabled(canSwitchState);
    ui->actionRunning_Mode_Cold_Wallet->setEnabled(canSwitchState);
    ui->actionBlock_Explorer->setEnabled(!isColdWallet);
    ui->actionWhite_papers->setEnabled(!isColdWallet);
    ui->actionGood_Money->setEnabled(!isColdWallet);
    ui->actionRoadmap->setEnabled(!isColdWallet);
    ui->actionMWC_website->setEnabled(!isColdWallet);
    ui->actionExchanges->setEnabled(!isColdWallet);
}


void MainWindow::on_actionSend_triggered()
{
    stateMachine->setActionWindow( state::STATE::SEND );
}

void MainWindow::on_actionExchanges_triggered()
{
    util->openUrlInBrowser("https://www.mwc.mw/exchanges");
}

void MainWindow::on_actionReceive_triggered()
{
    stateMachine->setActionWindow( state::STATE::RECEIVE_COINS );
}

void MainWindow::on_actionFinalize_triggered()
{
    stateMachine->setActionWindow( state::STATE::FINALIZE );
}

void MainWindow::on_actionTransactions_triggered()
{
    stateMachine->setActionWindow( state::STATE::TRANSACTIONS );
}

void MainWindow::on_actionListeners_triggered()
{
    stateMachine->setActionWindow( state::STATE::LISTENING );
}

void MainWindow::on_actionNode_Overview_triggered()
{
    stateMachine->setActionWindow( state::STATE::NODE_INFO );
}

void MainWindow::on_actionResync_with_full_node_triggered()
{
    if (control::MessageBox::questionText(this, "Re-sync account with a node",
            "Account re-sync will validate transactions and outputs for your accounts. Re-sync can take several minutes.\nWould you like to continue",
                       "No", "Yes",
                       "Cancel resync operation",
                       "I am agree to wait some time, lets go forward and resync",
                       true, false) == WndManager::RETURN_CODE::BTN2 ) {
        // Starting resync
        stateMachine->activateResyncState();
    }
}

void MainWindow::on_actionOutputs_triggered()
{
    stateMachine->setActionWindow( state::STATE::OUTPUTS );
}

void MainWindow::on_actionAirdrop_triggered()
{
   if ( QGuiApplication::queryKeyboardModifiers() & Qt::ShiftModifier ) {
       stateMachine->setActionWindow(state::STATE::SWAP);
   }
   else {
       stateMachine->setActionWindow(state::STATE::AIRDRDOP_MAIN);
   }
}

void MainWindow::on_actionHODL_triggered()
{
    stateMachine->setActionWindow( state::STATE::HODL );
}

void MainWindow::on_actionWallet_accounts_triggered()
{
    stateMachine->setActionWindow( state::STATE::ACCOUNTS );
}

void MainWindow::on_actionAccounts_triggered()
{
    stateMachine->setActionWindow( state::STATE::ACCOUNTS );
}

void MainWindow::on_actionContacts_triggered()
{
    stateMachine->setActionWindow( state::STATE::CONTACTS );
}

void MainWindow::on_actionShow_passphrase_triggered()
{
    QString passwordHash = wallet->getPasswordHash();

    if ( !passwordHash.isEmpty() ) {
        if (WndManager::RETURN_CODE::BTN2 !=
            control::MessageBox::questionText(this, "Wallet Password",
                                              "You are going to view wallet mnemonic passphrase.\n\nPlease input your wallet password to continue",
                                              "Cancel", "Confirm",
                                              "Cancel show mnemonic passphrase",
                                              "Verify the password and show the passphrase",
                                              false, true, 1.0,
                                              passwordHash, WndManager::RETURN_CODE::BTN2))
            return;
    }

    // passwordHash should contain raw password value form the messgage box
    stateMachine->activateShowSeed( passwordHash );
}

void MainWindow::on_actionEvent_log_triggered()
{
    stateMachine->setActionWindow( state::STATE::EVENTS );
}

void MainWindow::on_actionLogout_triggered()
{
    stateMachine->logout();
}

void MainWindow::on_actionConfig_triggered()
{
    stateMachine->setActionWindow( state::STATE::WALLET_CONFIG );
}

void MainWindow::on_actionRunning_Mode_Cold_Wallet_triggered()
{
    stateMachine->setActionWindow( state::STATE::WALLET_RUNNING_MODE );
}

void MainWindow::on_actionBlock_Explorer_triggered()
{
    util->openUrlInBrowser("https://explorer.mwc.mw/");
}

void MainWindow::on_actionWhite_papers_triggered()
{
    util->openUrlInBrowser("https://www.mwc.mw/whitepaper");
}

void MainWindow::on_actionGood_Money_triggered()
{
    util->openUrlInBrowser("https://www.mwc.mw/good-money");
}

void MainWindow::on_actionRoadmap_triggered()
{
    util->openUrlInBrowser("https://github.com/mwcproject/mwc-node/blob/master/doc/roadmap.md");
}

void MainWindow::on_actionMWC_website_triggered()
{
    util->openUrlInBrowser("https://www.mwc.mw/");
}

}

