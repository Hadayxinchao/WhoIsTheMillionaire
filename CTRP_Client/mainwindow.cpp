#include "mainwindow.h"
#include "qstyle.h"
#include "ui_mainwindow.h"
#include <QThread>
#include <QtMath>
#include <iostream>
#include <random>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// #define TESTING

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setupSpinBoxes();
    m_socket = nullptr;
    m_acc_name.clear();
    ui->grpMain->setEnabled(false);
    ui->grpSignIn->setEnabled(false);
    setCurrentTab(DISPLAY::MAIN);

    // Read product list:
    QFile file(":/resources/Products/products_list.txt");
    if (!file.open(QFile::ReadOnly | QFile::Text))
    {
        QMessageBox::critical(this, "SERVER", QString("Cannot open products_list.txt"));
        exit(EXIT_FAILURE);
    }
    m_product_list.push_back(Product("none", 0, "image_none.jpg"));
    QTextStream in(&file);
    QString _name, _price, _path, _id;
    while (!in.atEnd())
    {
        _id = in.readLine();
        _name = in.readLine();
        _price = in.readLine();
        _path = in.readLine();
        if(_id.isEmpty() || _name.isEmpty() || _price.isEmpty() || _path.isEmpty())
            break;
        m_product_list.push_back(Product(_name, _price.toInt(), _path));
        if (in.status() == QTextStream::ReadCorruptData)
            break;
    }
    file.close();

#ifdef TESTING
    ui->grpMain->setEnabled(true);
    m_score = 1000;
    m_acc_name = "TEST";
#endif
}

void MainWindow::setupSpinBoxes()
{
    QList<QSpinBox*> spinBoxes = {
        ui->quantityProduct1,
        ui->quantityProduct2, 
        ui->quantityProduct3,
        ui->quantityProduct4,
        ui->quantityProduct5
    };

    for(auto* spinBox : spinBoxes) {
        spinBox->setMinimum(0);
        spinBox->setMaximum(100000);
        spinBox->setSingleStep(10);
        spinBox->setValue(0);
        spinBox->setAlignment(Qt::AlignCenter);
    }
}

void MainWindow::setCurrentTab(int index) {
    if(index < 0 || index >= ui->tabWidget->count())
        return;
    for(int i = 0; i < ui->tabWidget->count(); i++)
        if(i != index)
            ui->tabWidget->setTabEnabled(i, false);
    ui->tabWidget->setTabEnabled(index, true);
    ui->tabWidget->setCurrentIndex(index);
}

void MainWindow::on_lnIPAddress_textChanged(const QString &arg1)
{
    QString state = "0";
    if (arg1 == "...") {
        state = "";
    } else {
        QHostAddress address(arg1);
        if (QAbstractSocket::IPv4Protocol == address.protocol()) {
            state = "1";
        }
    }
    ui->lnIPAddress->setProperty("state", state);
    style()->polish(ui->lnIPAddress);
}

MainWindow::~MainWindow()
{
    if(m_socket != nullptr && m_socket->isOpen()) {
        m_socket->close();
    }

    delete ui;
}

void MainWindow::on_btnConnect_clicked()
{
    // disconnect
    if(m_socket != nullptr) {
        m_socket->close();
        m_socket = nullptr;
        showLog("Disconnected to Server");
        ui->btnConnect->setText("Connect");
        ui->grpSignIn->setEnabled(false);
        ui->grpMain->setEnabled(false);
        m_acc_name.clear();
        ui->grpSignIn->setEnabled(false);
        ui->btnSignIn->setText("Sign In");
        return;
    }
    // connect
    auto ip = ui->lnIPAddress->text();
    auto port = ui->spnPort->value();
    m_socket = new QTcpSocket(this);

    connect(m_socket, &QTcpSocket::readyRead, this, &MainWindow::readSocketSlot);
    connect(m_socket, &QTcpSocket::disconnected, this, &MainWindow::discardSocket);
    connect(m_socket, &QAbstractSocket::errorOccurred, this, &MainWindow::displayError);

    m_socket->connectToHost(ip, port);
    if(m_socket->waitForConnected()) {
        showLog("Connected to Server");
        ui->btnConnect->setText("Disconnect");
        ui->grpSignIn->setEnabled(true);
        ui->goRegister->setEnabled(true);
        m_acc_name.clear();
        ui->grpSignIn->setEnabled(true);
    }
    else {
        showLog(QString("Error occurred: %1.").arg(m_socket->errorString()));
        delete m_socket;
        m_socket = nullptr;
    }
}

void MainWindow::on_btnSignIn_clicked()
{
    // log in
    if(m_acc_name.isEmpty()) {
        auto username = ui->usernameLineEdit->text().trimmed();
        auto password = ui->passwordLineEdit->text().trimmed();
        ui->passwordLineEdit->clear();
        if(username.isEmpty() || password.isEmpty())
            return;

        m_acc_name.clear();
        m_process_list.push_back(PROCESS_MODE::LOGIN);
        auto signInInfo = "LOGIN" + username + "|" + password;
        sendToServer(signInInfo);
        return;
    }
    // sign out
    sendToServer("LOGOU" + QString::number(m_score));
    m_acc_name.clear();
    ui->grpMain->setEnabled(false);
    ui->btnSignIn->setText("Sign In");
    ui->goRegister->setEnabled(true);
    ui->idInp->clear();
    ui->scoreInp->clear();
    ui->usernameLineEdit->setEnabled(true);
    ui->passwordLineEdit->setEnabled(true);
}

void MainWindow::on_btnRegister_clicked()
{
    auto username = ui->usernameSignup->text().trimmed();
    auto password = ui->passwordSignup->text().trimmed();
    auto cfpassword = ui->cfPasswordSignup->text().trimmed();
    ui->usernameSignup->clear();
    ui->passwordSignup->clear();
    ui->cfPasswordSignup->clear();
    if(username.isEmpty() || password.isEmpty() || cfpassword.isEmpty())
    {
        QMessageBox::warning(this, "Registration Error", "Please fill in all the fields.");
        return;
    }
    m_process_list.push_back(PROCESS_MODE::SIGNUP);
    auto registerInfo = "SIGNU" + username + "|" + password + "|" + cfpassword;
    sendToServer(registerInfo);
    return;
}

void MainWindow::on_goRegister_clicked()
{
    setCurrentTab(DISPLAY::SIGN_UP);
    ui->goRegister->setEnabled(false);
    ui->grpSignIn->setEnabled(false);
}


void MainWindow::on_btnCancelRegister_clicked()
{
    setCurrentTab(DISPLAY::MAIN);
    ui->grpSignIn->setEnabled(true);
    ui->goRegister->setEnabled(true);
    ui->btnSignIn->setEnabled(true);
}


void MainWindow::on_btnViewRank_clicked()
{
    setCurrentTab(DISPLAY::VIEW_RANKING);
    m_process_list.push_back(PROCESS_MODE::ViewRank);
    sendToServer("VIEWR");
}

void MainWindow::readSocketSlot() {
    QByteArray buffer;

    QDataStream socketStream(m_socket);
    socketStream.setVersion(QDataStream::Qt_5_15);

    socketStream.startTransaction();
    socketStream >> buffer;

    QString response = QString::fromStdString(buffer.toStdString());
    if(m_process_list.isEmpty()) return;
    int mode = m_process_list.first();
    m_process_list.pop_front();
    if(mode == PROCESS_MODE::LOGIN) {
        if(response.left(5) == "SUCLI") {
            m_acc_name = ui->usernameLineEdit->text().trimmed();
            m_score = response.mid(7).toInt();
            ui->grpMain->setEnabled(true);
            ui->btnSignIn->setText("Sign Out");
            showLog(QString("My score is %1").arg(m_score));
            ui->idInp->setText(m_acc_name);
            ui->scoreInp->setText(QString::number(m_score));
            ui->usernameLineEdit->setEnabled(false);
            ui->passwordLineEdit->setEnabled(false);
            ui->goRegister->setEnabled(false);
        }
        else {
            showLog(response);
        }
    }
    else if (mode == PROCESS_MODE::SIGNUP) {
        if(response.left(5) == "SIGNU") {
            showLog("New account created");
            ui->grpSignIn->setEnabled(true);
            setCurrentTab(0);
        }
        else if (response == "CFPNM")
            showLog("Confirm password not match");
        else {
            showLog(response);
        }
    }
    else if (mode == PROCESS_MODE::ViewRank) {
        QStringList lstr = response.split('|');
        QLabel* nameLabels[10] = {ui->nameTop1, ui->nameTop2, ui->nameTop3, ui->nameTop4, ui->nameTop5, ui->nameTop6, ui->nameTop7, ui->nameTop8, ui->nameTop9, ui->nameTop10};
        QLabel* scoreLabels[10] = {ui->scoreTop1, ui->scoreTop2, ui->scoreTop3, ui->scoreTop4, ui->scoreTop5, ui->scoreTop6, ui->scoreTop7, ui->scoreTop8, ui->scoreTop9, ui->scoreTop10};
        for (int i = 0; i < 10; ++i) {
            if (i*2 + 1 < lstr.size()) {
                nameLabels[i]->setText(lstr.at(i*2));
                scoreLabels[i]->setText(lstr.at(i*2 + 1));
            }
        }
    }
    else if(mode == PROCESS_MODE::GoRom) {
        if(response.left(5) == "READY") {
            QStringList parts = response.mid(5).split("|");
            if(parts.size() == 2) {
                ui->player_name_2->setText(parts[0]);
                ui->player2_score->setText(parts[1]);
                ui->btnStartPvP->setEnabled(true);
                ui->roomStatus->setText("Ready to battle!");
            }
        }
        else {
            showLog(response);
        }
    }
    else if(mode == PROCESS_MODE::ReadyPvP) {
        if(response == "START_PVP")
            setupME();
    }
    else if(mode == PROCESS_MODE::FinishPvP) {
        if(response.left(5) == "RSPVP") {
            int p2_score = response.mid(5).toInt();
            ui->player2_score->setText(QString::number(p2_score));
            if(m_p2p_score > p2_score) {
                m_score += m_bet;
                ui->roomStatus->setText("You WIN :)");
            }
            else if(m_p2p_score < p2_score) {
                m_score -= m_bet;
                ui->roomStatus->setText("You LOSE :(");
            }
            else {
               ui->roomStatus->setText("DRAW!");
            }
            ui->scoreInp->setText(QString::number(m_score));
            ui->btnBackToMenu_Room->setVisible(true);
            ui->btnBackToMenu_Room->setEnabled(true);
        }
    }
}

void MainWindow::on_btnExitRoom_clicked() {
    if(ui->player_name_2->text() == "...") {
        sendToServer("OUTRM");
        if(m_process_list.last() == PROCESS_MODE::GoRom)
            m_process_list.pop_back();
    }
    setCurrentTab(DISPLAY::MAIN);
}

void MainWindow::discardSocket()
{
    m_socket->deleteLater();
    m_socket = nullptr;
    showLog("Disconnected to Server!");
}

void MainWindow::displayError(QAbstractSocket::SocketError socketError)
{
    switch (socketError) {
    case QAbstractSocket::RemoteHostClosedError:
        break;
    case QAbstractSocket::HostNotFoundError:
        showLog("The host was not found. Please check the host name and port settings.");
        break;
    case QAbstractSocket::ConnectionRefusedError:
        showLog("The connection was refused by the peer. Make sure QTCPServer is running, and check that the host name and port settings are correct.");
        break;
    default:
        showLog(QString("The following error occurred: %1.").arg(m_socket->errorString()));
        break;
    }
}

void MainWindow::sendToServer(QString str)
{
    if(m_socket != nullptr)
    {
        if(m_socket->isOpen())
        {
            QDataStream socketStream(m_socket);
            socketStream.setVersion(QDataStream::Qt_5_15);

            QByteArray byteArray = str.toUtf8();
            socketStream << byteArray;
            m_socket->waitForBytesWritten();
        }
        else
            showLog("Socket doesn't seem to be opened");
    }
    else
        showLog("Not connected");
}

void MainWindow::showLog(const QString& str)
{
    ui->logConsole->append(str);
}

void MainWindow::on_btnSingle_clicked() {
    m_p2p_mode = false;
    setupME();
}

void MainWindow::on_btnPvP_clicked() {
    m_p2p_mode = true;
    m_p2p_score = 0;
    ui->roomGRP->setVisible(false);
    ui->BETGRP->setEnabled(true);
    ui->btnBET100->setStyleSheet("background-color: lightGray");
    ui->btnBET200->setStyleSheet("background-color: lightGray");
    ui->btnBET500->setStyleSheet("background-color: lightGray");
    if(m_score >= 100)
        ui->btnBET100->setEnabled(true);
    else
        ui->btnBET100->setEnabled(false);
    if(m_score >= 200)
        ui->btnBET200->setEnabled(true);
    else
        ui->btnBET200->setEnabled(false);
    if(m_score >= 500)
        ui->btnBET500->setEnabled(true);
    else
        ui->btnBET500->setEnabled(false);
    setCurrentTab(DISPLAY::ROOM);
}

void MainWindow::setupInRoom(int bet) {
    ui->BETGRP->setEnabled(false);
    ui->roomGRP->setVisible(true);
    ui->roomStatus->setText("Waiting for other player...");
    ui->btnStartPvP->setEnabled(false);
    ui->player_name_1->setText(m_acc_name);
    ui->player_name_2->setText("...");
    ui->player1_score->setText(QString::number(m_score));
    ui->player2_score->setText("...");
    ui->btnBackToMenu_Room->setVisible(true);
    ui->btnBackToMenu_Room->setEnabled(true);
    sendToServer("GROOM" + QString::number(bet));
    m_process_list.push_back(PROCESS_MODE::GoRom);
}

void MainWindow::on_btnBET100_clicked() {
    ui->btnBET100->setStyleSheet("background-color: red");
    m_bet = 100;
    setupInRoom(100);
}

void MainWindow::on_btnBET200_clicked() {
    ui->btnBET200->setStyleSheet("background-color: red");
    m_bet = 200;
    setupInRoom(200);
}

void MainWindow::on_btnBET500_clicked() {
    ui->btnBET500->setStyleSheet("background-color: red");
    m_bet = 500;
    setupInRoom(500);
}

void MainWindow::on_btnSubmitME_clicked() {
    if(ui->choseME->value() == m_solution_ME) {
        QMessageBox::information(this, "RESULT", QString("Congrats you made the right choice!"));
        if(m_p2p_mode) {
            m_p2p_score += 500;
        }
        else {
            m_score += 500;
            ui->scoreInp->setText(QString::number(m_score));
        }
    }
    else {
        QMessageBox::information(this, "RESULT", QString("Wish you luck next time!"));
    }
    setupGrocery();
}

void MainWindow::on_btnSubmitG_clicked() {
    int userAmount[5] = {ui->quantityProduct1->value(),
                         ui->quantityProduct2->value(),
                         ui->quantityProduct3->value(),
                         ui->quantityProduct4->value(),
                         ui->quantityProduct5->value()};
    int userTotal = 0;
    for(int i = 0; i < 5; i++) {
        userTotal += (userAmount[i] * m_product_list[m_ID_G[i]].price);
    }
    if((int)abs(userTotal - m_total_G) <= 2000) {
        QMessageBox::information(this, "RESULT", QString("Your purchase: %1\nCongrats you made the right choice!").arg(userTotal));
        if(m_p2p_mode) {
            m_p2p_score += 500;
        }
        else {
            m_score += 500;
            ui->scoreInp->setText(QString::number(m_score));
        }
    }
    else {
        QMessageBox::information(this, "RESULT", QString("Your purchase: %1\nWish you luck next time!").arg(userTotal));
    }
    setupDP();
    // finish game
//    if(m_p2p_mode) {
//        ui->player1_score->setText(QString::number(m_p2p_score));
//        ui->roomStatus->setText("Waiting for your competitor finish game...");
//        ui->btnStartPvP->setEnabled(false);
//        sendToServer("RSPVP" + QString::number(m_p2p_score));
//        m_process_list.push_back(PROCESS_MODE::FinishPvP);
//        setupSpinWheel();
//    }
//    else
//        setupSpinWheel();
}

void MainWindow::on_btnSubmitDP_clicked() {
    QCheckBox* check[4] = {ui->check1_DP, ui->check2_DP, ui->check3_DP, ui->check4_DP};
    int cntCheck = 0;
    int uncheckIndex = -1;
    for(int i = 0; i < 4; i++) {
        if(check[i]->isChecked())
            cntCheck++;
        else
            uncheckIndex = i;
    }
    if(cntCheck != 3) {
        QMessageBox::warning(this, "WARNING", QString("Please select 3 products!"));
        return;
    }
    if(uncheckIndex == m_index_DP) {
        QMessageBox::information(this, "RESULT", QString("Congrats you made the right choice!"));
        if(m_p2p_mode) {
            m_p2p_score += 500;
        }
        else {
            m_score += 500;
            ui->scoreInp->setText(QString::number(m_score));
        }
    }
    else {
        QMessageBox::information(this, "RESULT", QString("Wish you luck next time!"));
    }
    setupSpinWheel();
    // finish game
//    if(m_p2p_mode) {
//        ui->player1_score->setText(QString::number(m_p2p_score));
//        ui->roomStatus->setText("Waiting for your competitor finish game...");
//        ui->btnStartPvP->setEnabled(false);
//        sendToServer("RSPVP" + QString::number(m_p2p_score));
//        m_process_list.push_back(PROCESS_MODE::FinishPvP);
//        setCurrentTab(DISPLAY::ROOM);
//    }
//    else
//        setCurrentTab(DISPLAY::MAIN);
}

void MainWindow::on_btnStartPvP_clicked() {
    sendToServer("READY");
    m_process_list.push_back(PROCESS_MODE::ReadyPvP);
    ui->btnStartPvP->setEnabled(false);
}

int MainWindow::randomProductIndex() {
    return ((rand() % (m_product_list.size() - 1)) + 1);
}

void MainWindow::setupME() {
    int id1 = randomProductIndex();
    int id2 = randomProductIndex();
    int id3 = randomProductIndex();
    while(id2 == id1) {
        id2 = randomProductIndex();
    }
    while(id3 == id1 || id3 == id2) {
        id3 = randomProductIndex();
    }

    ui->label_pic1->setPixmap(QPixmap(":/resources/Products/images/" + m_product_list[id1].imgPath).scaled(200, 200, Qt::KeepAspectRatio));
    ui->label_name1->setText(m_product_list[id1].name);
    ui->label_pic2->setPixmap(QPixmap(":/resources/Products/images/" + m_product_list[id2].imgPath).scaled(200, 200, Qt::KeepAspectRatio));
    ui->label_name2->setText(m_product_list[id2].name);
    ui->label_pic3->setPixmap(QPixmap(":/resources/Products/images/" + m_product_list[id3].imgPath).scaled(200, 200, Qt::KeepAspectRatio));
    ui->label_name3->setText(m_product_list[id3].name);
    if(m_product_list[id1].price > m_product_list[id2].price && m_product_list[id1].price > m_product_list[id3].price)
        m_solution_ME = 1;
    else if(m_product_list[id2].price > m_product_list[id1].price && m_product_list[id2].price > m_product_list[id3].price)
        m_solution_ME = 2;
    else
        m_solution_ME = 3;
    ui->choseME->setValue(1);
    ui->btnBackToMenu_ME->setEnabled(true);
    ui->btnBackToMenu_ME->setEnabled(true);
    setCurrentTab(DISPLAY::MOST_EXPENSIVE);
}

void MainWindow::setupGrocery() {
    for(int i = 0; i < 5; i++) {
        m_ID_G[i] = randomProductIndex();
        for(int j = 0; j < i; j++) {
            if(m_ID_G[i] == m_ID_G[j]) {
                i--;
                break;
            }
        }
    }
    m_total_G = 0;
    int amount;
    QLabel* pic[5] = {ui->labelG_pic1, ui->labelG_pic2, ui->labelG_pic3, ui->labelG_pic4, ui->labelG_pic5};
    QLabel* name[5] = {ui->labelG_name1, ui->labelG_name2, ui->labelG_name3, ui->labelG_name4, ui->labelG_name5};
    for(int i = 0; i < 5; i++) {
        amount = (rand() % 3) + 1;
        m_total_G += (amount * m_product_list[m_ID_G[i]].price);
        pic[i]->setPixmap(QPixmap(":/resources/Products/images/" + m_product_list[m_ID_G[i]].imgPath).scaled(120, 120, Qt::KeepAspectRatio));
        name[i]->setText(m_product_list[m_ID_G[i]].name);
    }
    ui->rangeInp->setText(QString("From %1 to %2 thousand Dong").arg(m_total_G-2000).arg(m_total_G+2000));
    ui->quantityProduct1->setValue(0);
    ui->quantityProduct2->setValue(0);
    ui->quantityProduct3->setValue(0);
    ui->quantityProduct4->setValue(0);
    ui->quantityProduct5->setValue(0);
    ui->btnBackToMenu_GC->setEnabled(true);
    ui->btnBackToMenu_GC->setEnabled(true);
    setCurrentTab(DISPLAY::GROCERY);
}

void MainWindow::setupDP() {
    int indexList[4];
    for(int i = 0; i < 4; i++) {
        indexList[i] = randomProductIndex();
        for(int j = 0; j < i; j++) {
            if(indexList[i] == indexList[j]) {
                i--;
                break;
            }
        }
    }
    m_index_DP = rand() % 4;
    QLabel* pic[4] = {ui->pic1_DP, ui->pic2_DP, ui->pic3_DP, ui->pic4_DP};
    QLabel* name[4] = {ui->name1_DP, ui->name2_DP, ui->name3_DP, ui->name4_DP};
    QCheckBox* check[4] = {ui->check1_DP, ui->check2_DP, ui->check3_DP, ui->check4_DP};
    for(int i = 0; i < 4; i++) {
        pic[i]->setPixmap(QPixmap(":/resources/Products/images/" + m_product_list[indexList[i]].imgPath).scaled(150, 150, Qt::KeepAspectRatio));
        name[i]->setText(m_product_list[indexList[i]].name);
        check[i]->setChecked(false);
    }
    ui->rangeInp_DP->setText(QString("Given price: %1 thousand Dong").arg(m_product_list[indexList[m_index_DP]].price));
    ui->btnBackToMenu_SP->setEnabled(true);
    ui->btnBackToMenu_SP->setEnabled(true);
    setCurrentTab(DISPLAY::DANGER_PRICE);
}

void MainWindow::setupSpinWheel()
{
    ui->firstPoint->clear();
    ui->secondPoint->clear();
    ui->textFinalBonus->clear();
    ui->btnSubmitLW->setEnabled(false);
    ui->btnSpinWheel->setEnabled(true);
    ui->btnBackToMenu_LW->setEnabled(true);
    ui->btnBackToMenu_LW->setEnabled(true);
    setCurrentTab(DISPLAY::SPIN_WHEEL);
}

void MainWindow::setupTSC()
{
    int id1 = randomProductIndex();
    int id2 = randomProductIndex();
    int id3 = randomProductIndex();
    while(id2 == id1) {
        id2 = randomProductIndex();
    }
    while(id3 == id1 || id3 == id2) {
        id3 = randomProductIndex();
    }

    ui->label_show_case_pic1->setPixmap(QPixmap(":/resources/Products/images/" + m_product_list[id1].imgPath).scaled(200, 200, Qt::KeepAspectRatio));
    ui->label_show_case_price1->setText(m_product_list[id1].name);
    ui->label_show_case_price1->setText(QString::number(m_product_list[id1].price));
    ui->label_show_case_price1->hide();
    ui->label_show_case_pic2->setPixmap(QPixmap(":/resources/Products/images/" + m_product_list[id2].imgPath).scaled(200, 200, Qt::KeepAspectRatio));
    ui->label_show_case_price2->setText(QString::number(m_product_list[id2].price));
    ui->label_show_case_price2->hide();
    ui->label_show_case_pic3->setPixmap(QPixmap(":/resources/Products/images/" + m_product_list[id3].imgPath).scaled(200, 200, Qt::KeepAspectRatio));
    ui->label_show_case_name3->setText(m_product_list[id3].name);
    ui->label_show_case_price3->setText(QString::number(m_product_list[id3].price));
    ui->label_show_case_price3->hide();
    if(m_product_list[id1].price > m_product_list[id2].price && m_product_list[id1].price > m_product_list[id3].price)
        m_solution_ME = 1;
    else if(m_product_list[id2].price > m_product_list[id1].price && m_product_list[id2].price > m_product_list[id3].price)
        m_solution_ME = 2;
    else
        m_solution_ME = 3;
    ui->totalPrice->setValue(0);
    ui->btnSubmitEndGame->setEnabled(false);
    ui->btnBackToMenu_SC->setEnabled(false);
    setCurrentTab(DISPLAY::SHOW_CASE);
}


void MainWindow::on_btnSpinWheel_clicked()
{
    std::vector<int> numbers;
    for (int i = 5; i <= 100; i += 5) {
        numbers.push_back(i);
    }

    // Seed the random number generator
    std::random_device rd;
    std::mt19937 gen(rd());

    // Generate a random index to select a number from the list
    std::uniform_int_distribution<> dis(0, numbers.size() - 1);
    int randomIndex = dis(gen);

    // Get the random number from the list based on the random index
    int randomNum = numbers[randomIndex];
    if (randomNum == 100 && ui->firstPoint->toPlainText() == "") {
        bonus = randomNum*10;
        ui->firstPoint->setText(QString::number(bonus));
        ui->textFinalBonus->setText(QString::number(bonus));
        ui->btnSpinWheel->setEnabled(false);
        ui->btnSubmitLW->setEnabled(true);
   } else {
        if (ui->firstPoint->toPlainText() == "") {
            ui->firstPoint->setText(QString::number(randomNum));
        } else {
            ui->secondPoint->setText(QString::number(randomNum));
            if((randomNum+ui->firstPoint->toPlainText().toInt()) == 100) {
                bonus = 100*10;
            } else if((randomNum+ui->firstPoint->toPlainText().toInt()) > 100) {
                bonus = qAbs(randomNum+ui->firstPoint->toPlainText().toInt()-100)*10;
            } else {
                bonus = qAbs(randomNum+ui->firstPoint->toPlainText().toInt())*10;
            }
            ui->textFinalBonus->setText(QString::number(bonus));
            ui->btnSpinWheel->setEnabled(false);
            ui->btnSubmitLW->setEnabled(true);
        }
    }
}

void MainWindow::on_btnSubmitLW_clicked()
{
    if(m_p2p_mode) {
        m_p2p_score += bonus;
    } else {
        m_score += bonus;
        ui->scoreInp->setText(QString::number(m_score));
    }
    setupTSC();
}

void MainWindow::on_btnSubmitEndGame_clicked()
{
    ui->btnSubmitEndGame->setEnabled(false);
    ui->btnBackToMenu_SC->setEnabled(true);

    int totalPrice = ui->totalPrice->text().toInt();
    int price1 = ui->label_show_case_price1->text().toInt();
    int price2 = ui->label_show_case_price2->text().toInt();
    int price3 = ui->label_show_case_price3->text().toInt();
    bonus = 0;
    if (qAbs(price1+price2+price3-totalPrice) <= 3000) {
        QMessageBox::information(this, "RESULT", QString("Congratulations!!! You win: %1\nWish you luck next time!"));
        ui->label_show_case_price1->show();
        ui->label_show_case_price2->show();
        ui->label_show_case_price3->show();
        ui->btnBackToMenu_SC->setEnabled(true);
        if (m_p2p_mode) {
            m_p2p_score += 2000;
        } else {
            m_score += 2000;
        }
    } else {
        QMessageBox::information(this, "RESULT", QString("You lose. Wish you luck next time!"));
        ui->label_show_case_price1->show();
        ui->label_show_case_price2->show();
        ui->label_show_case_price3->show();
        ui->btnBackToMenu_SC->setEnabled(true);
    }

    if(m_p2p_mode) {
        m_score += m_p2p_score;
        ui->scoreInp->setText(QString::number(m_score));
        ui->player1_score->setText(QString::number(m_p2p_score));
        ui->roomStatus->setText("Waiting for your competitor finish game...");
        ui->btnStartPvP->setEnabled(false);
        sendToServer("RSPVP" + QString::number(m_p2p_score));
        m_process_list.push_back(PROCESS_MODE::FinishPvP);
        setCurrentTab(DISPLAY::ROOM);
    }
    else {
        sendToServer("ENDGA" + QString::number(m_score));
    }

}

void MainWindow::on_totalPrice_valueChanged()
{
    if (ui->totalPrice->text().toInt() != 0) {
        ui->btnSubmitEndGame->setEnabled(true);
    } else {
        ui->btnSubmitEndGame->setEnabled(false);
    }
}

void MainWindow::on_btnBackToMenu_ME_clicked() {
    setCurrentTab(DISPLAY::MAIN);
}

void MainWindow::on_btnBackToMenu_GC_clicked() {
    setCurrentTab(DISPLAY::MAIN);
}

void MainWindow::on_btnBackToMenu_SP_clicked() {
    setCurrentTab(DISPLAY::MAIN);
}

void MainWindow::on_btnBackToMenu_LW_clicked() {
    setCurrentTab(DISPLAY::MAIN);
}

void MainWindow::on_btnBackToMenu_SC_clicked()
{
    setCurrentTab(DISPLAY::MAIN);
}

void MainWindow::on_btnBackToMenu_Room_clicked()
{
    setCurrentTab(DISPLAY::MAIN);
}

void MainWindow::on_btnBackToMenu_Ranking_clicked()
{
    setCurrentTab(DISPLAY::MAIN);
}

void MainWindow::on_btnClear_clicked()
{
    ui->logConsole->clear();
}
