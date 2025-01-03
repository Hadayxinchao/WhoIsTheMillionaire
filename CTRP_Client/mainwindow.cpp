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
#include <fstream>
#include <curl/curl.h>
#include <json/json.h>
#include <iostream>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

// #define TESTING

// Callback để ghi dữ liệu từ API vào chuỗi
size_t MainWindow::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Hàm lấy dữ liệu từ DummyJSON API
std::string MainWindow::fetchProductsData() {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "https://dummyjson.com/products?limit=30");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            std::cerr << "cURL Error: " << curl_easy_strerror(res) << std::endl;
            return "";
        }
    }
    return readBuffer;
}

void MainWindow::saveProductsToFile(const std::string& jsonData) {
    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(jsonData, root)) {
        showLog("Failed to parse JSON data!");
        return;
    }

    const Json::Value products = root["products"];

    // Read product list:
    
    QString _name, _price, _path, _id;

    for (unsigned int i = 0; i < products.size(); ++i) {
        QString name = QString::fromStdString(products[i]["title"].asString());
        QString price = QString::fromStdString(products[i]["price"].asString());
        QString imgPath = QString::fromStdString(products[i]["thumbnail"].asString());
        QString id = QString::fromStdString(products[i]["id"].asString());

        _id = id;
        _name = name;
        _price = price;
        _path = imgPath;
        if(_id.isEmpty() || _name.isEmpty() || _price.isEmpty() || _path.isEmpty())
            break;
        m_product_list.push_back(Product(_name, qRound(_price.toDouble()), _path));
    }
}

void MainWindow::loadImageToLabel(QLabel *label, const QString &imgPath) {
    if (imgPath.startsWith("http")) { 
        // Nếu là URL
        QNetworkRequest request((QUrl(imgPath))); 
        QNetworkAccessManager *manager = new QNetworkAccessManager(this);

        connect(manager, &QNetworkAccessManager::finished, this, [=](QNetworkReply *reply) {
            if (reply->error() == QNetworkReply::NoError) {
                QByteArray imageData = reply->readAll();
                QPixmap pixmap;
                if (pixmap.loadFromData(imageData)) {
                    label->setPixmap(pixmap.scaled(200, 200, Qt::KeepAspectRatio));
                } else {
                    showLog("Failed to load image from data");
                }
            } else {
                showLog("Network error: " + reply->errorString());
            }
            reply->deleteLater();
            manager->deleteLater();
        });

        manager->get(request);
    } else {
        QPixmap pixmap(imgPath);
        if (!pixmap.isNull()) {
            label->setPixmap(pixmap.scaled(200, 200, Qt::KeepAspectRatio));
        } else {
            showLog("Failed to load local image");
        }
    }
}

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

    // Fetch data from DummyJSON API
    std::string data = fetchProductsData();

    if (!data.empty()) {
        saveProductsToFile(data);
    } else {
        showLog("Failed to fetch product data!");
    }

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
    qDebug() << "Received response:" << response;
    if(response.left(5) == "REFPL") {
        QStringList lstr = response.mid(5).split('|');
        updateOnlinePlayers(lstr);
        return;
    }
    else if(response.left(5) == "INVIT") {
        QString inviter = response.mid(5).split('|')[0];
        int bet = response.mid(5).split('|')[1].toInt();
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Game Invitation",
                                    inviter + " invited you to play!\nDo you accept?",
                                    QMessageBox::Yes|QMessageBox::No);
        
        if (reply == QMessageBox::Yes) {
            m_bet = bet;
            if (m_score < bet) {
                QMessageBox::warning(this, "Game Invitation", "You don't have enough score to play this match!");
                return;
            } else {
                setupInRoom(bet);
                setCurrentTab(DISPLAY::ROOM);
            }

        } else {
            sendToServer("RJTJR" + inviter);
        }
        return;
    }
    else if (response.left(5) == "SURLS") {
        m_score -= m_bet;
        ui->scoreInp->setText(QString::number(m_score));
        QMessageBox::information(this, "Game Over", "You surrendered and lost the match!");
        setCurrentTab(DISPLAY::MAIN);
        return;
    }
    else if(response.left(5) == "SURWN") {
        m_score += m_bet;
        ui->scoreInp->setText(QString::number(m_score));
        QMessageBox::information(this, "Game Over", "Your opponent surrendered - You win!");
        setCurrentTab(DISPLAY::MAIN);
        return;
    }
    else if(response.left(5) == "OUTRM") {
        QString leaver = response.mid(5);
        QMessageBox::information(this, "Player Left",
                               leaver + " has left the room!");
        ui->player_name_2->setText("...");
        ui->player2_score->setText("...");
        ui->roomStatus->setText("Waiting for another player");
        ui->btnStartPvP->setEnabled(false);
        return;
    }
    if (m_process_list.isEmpty()) {
        if(response.left(5) == "READY") {
            m_process_list.push_back(PROCESS_MODE::GoRom);
        }
    }
    int mode = m_process_list.first();
    qDebug() << "Current process mode queue:" << m_process_list;
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
        if(response == "START_PVP") {
            in_pvp = true;
            setupME();
        }
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
    ui->roomGRP->setVisible(false);
    ui->BETGRP->setEnabled(true);
    ui->btnBET1000->setStyleSheet("background-color: lightGray");
    ui->btnBET2000->setStyleSheet("background-color: lightGray");
    ui->btnBET5000->setStyleSheet("background-color: lightGray");

    if(m_score >= 1000)
        ui->btnBET1000->setEnabled(true);
    else
        ui->btnBET1000->setEnabled(false);
    if(m_score >= 2000)
        ui->btnBET2000->setEnabled(true);
    else
        ui->btnBET2000->setEnabled(false);
    if(m_score >= 5000)
        ui->btnBET5000->setEnabled(true);
    else
        ui->btnBET5000->setEnabled(false);
    setCurrentTab(DISPLAY::ROOM);
}

void MainWindow::setupInRoom(int bet) {
    ui->btnBET1000->setStyleSheet("background-color: lightGray");
    ui->btnBET2000->setStyleSheet("background-color: lightGray");
    ui->btnBET5000->setStyleSheet("background-color: lightGray");
    if (bet == 1000) {
        ui->btnBET1000->setStyleSheet("background-color: #1E90FF");
    } else if (bet == 2000) {
        ui->btnBET2000->setStyleSheet("background-color: #1E90FF");
    } else if (bet == 5000) {
        ui->btnBET5000->setStyleSheet("background-color: #1E90FF");
    }
    m_bet = bet;
    m_has_room = true;
    showLog("Setting up room..." + QString::number(bet));
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

void MainWindow::on_btnBET1000_clicked() {
    setupInRoom(1000);
}

void MainWindow::on_btnBET2000_clicked() {
    setupInRoom(2000);
}

void MainWindow::on_btnBET5000_clicked() {
    setupInRoom(5000);
}

void MainWindow::on_btnSubmitME_clicked() {
    ui->label_most_expensive_price1->show();
    ui->label_most_expensive_price2->show();
    ui->label_most_expensive_price3->show();
    
    if(ui->choseME->value() == m_solution_ME) {
        QMessageBox::information(this, "RESULT", QString("Congrats you made the right choice!"));
        if(m_p2p_mode) {
            m_p2p_score += 500;
            ui->singleScore_GC->setText(QString::number(m_p2p_score) + " points");
        }
        else {
            m_single_score += 500;
            ui->singleScore_GC->setText(QString::number(m_single_score) + " points");
            m_score += 500;
            ui->scoreInp->setText(QString::number(m_score));
        }
    }
    else {
        ui->singleScore_GC->setText(QString::number(m_single_score) + " points");
        QMessageBox::information(this, "RESULT", QString("Your answer is wrong! Wish you luck next time!"));
    }
    setupGrocery();
}

void MainWindow::on_btnSubmitG_clicked() {
    ui->label_grocery_price1->show();
    ui->label_grocery_price2->show();
    ui->label_grocery_price3->show();
    ui->label_grocery_price4->show();
    ui->label_grocery_price5->show();

    int userAmount[5] = {ui->quantityProduct1->value(),
                         ui->quantityProduct2->value(),
                         ui->quantityProduct3->value(),
                         ui->quantityProduct4->value(),
                         ui->quantityProduct5->value()};
    int userTotal = 0;
    for(int i = 0; i < 5; i++) {
        userTotal += (userAmount[i] * m_product_list[m_ID_G[i]].price);
    }
    if((int)abs(userTotal - m_total_G) <= 20) {
        QMessageBox::information(this, "RESULT", QString("Your purchase: %1\nCongrats you made the right choice!").arg(userTotal));
        if(m_p2p_mode) {
            m_p2p_score += 500;
            ui->singleScore_DP->setText(QString::number(m_p2p_score) + " points");
        }
        else {
            m_single_score += 500;
            ui->singleScore_DP->setText(QString::number(m_single_score) + " points");
            m_score += 500;
            ui->scoreInp->setText(QString::number(m_score));
        }
    }
    else {
        ui->singleScore_DP->setText(QString::number(m_single_score) + " points");
        QMessageBox::information(this, "RESULT", QString("Your purchase: %1\n Your answer is wrong!").arg(userTotal));
    }
    setupDP();
}

void MainWindow::on_btnSubmitDP_clicked() {
    ui->label_dp_price1->show();
    ui->label_dp_price2->show();
    ui->label_dp_price3->show();
    ui->label_dp_price4->show();

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
            ui->singleScore_SC->setText(QString::number(m_p2p_score) + " points");
        }
        else {
            m_single_score += 500;
            ui->singleScore_SC->setText(QString::number(m_single_score) + " points");
            m_score += 500;
            ui->scoreInp->setText(QString::number(m_score));
        }
    }
    else {
        ui->singleScore_SC->setText(QString::number(m_single_score) + " points");
        QMessageBox::information(this, "RESULT", QString("Your answer is wrong! Wish you luck next time!"));
    }
    setupSpinWheel();
}

void MainWindow::on_btnStartPvP_clicked() {
    sendToServer("READY");
    m_process_list.push_back(PROCESS_MODE::ReadyPvP);
    ui->btnStartPvP->setEnabled(false);
}

int MainWindow::randomProductIndex() {
    if (m_product_list.size() <= 1) {
        return 0; // or throw exception
    }
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, m_product_list.size() - 1);
    return dis(gen);
}

void MainWindow::setupME() {
    ui->singleScore_ME->setText("0 points");
    int id1 = randomProductIndex();
    int id2 = randomProductIndex();
    int id3 = randomProductIndex();
    while (id2 == id1) {
        id2 = randomProductIndex();
    }
    while (id3 == id1 || id3 == id2) {
        id3 = randomProductIndex();
    }

    // Thiết lập ảnh, giá và tên sản phẩm
    QLabel* labels_pic[] = {ui->label_pic1, ui->label_pic2, ui->label_pic3};
    QLabel* labels_name[] = {ui->label_name1, ui->label_name2, ui->label_name3};
    QLabel* labels_price[] = {ui->label_most_expensive_price1, ui->label_most_expensive_price2, ui->label_most_expensive_price3};
    int ids[] = {id1, id2, id3};

    for (int i = 0; i < 3; ++i) {
        loadImageToLabel(labels_pic[i], m_product_list[ids[i]].imgPath);
        labels_name[i]->setText(m_product_list[ids[i]].name);
        labels_price[i]->setText(QString::number(m_product_list[ids[i]].price) + " USD");
        labels_price[i]->hide();
    }
    
    // Tìm sản phẩm có giá cao nhất
    if (m_product_list[id1].price > m_product_list[id2].price && m_product_list[id1].price > m_product_list[id3].price)
        m_solution_ME = 1;
    else if (m_product_list[id2].price > m_product_list[id1].price && m_product_list[id2].price > m_product_list[id3].price)
        m_solution_ME = 2;
    else
        m_solution_ME = 3;

    ui->choseME->setValue(1);
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
    QLabel* labels_price[5] = {ui->label_grocery_price1, ui->label_grocery_price2, ui->label_grocery_price3, ui->label_grocery_price4, ui->label_grocery_price5};

    for (int i = 0; i < 5; i++) {
        amount = (rand() % 3) + 1;
        m_total_G += (amount * m_product_list[m_ID_G[i]].price);
        loadImageToLabel(pic[i], m_product_list[m_ID_G[i]].imgPath);
        name[i]->setText(m_product_list[m_ID_G[i]].name);
        labels_price[i]->setText(QString::number(m_product_list[m_ID_G[i]].price) + " USD");
        labels_price[i]->hide();
    }

    ui->rangeInp->setText(QString("From %1 to %2 USD").arg(m_total_G-20).arg(m_total_G+20));
    ui->quantityProduct1->setValue(0);
    ui->quantityProduct2->setValue(0);
    ui->quantityProduct3->setValue(0);
    ui->quantityProduct4->setValue(0);
    ui->quantityProduct5->setValue(0);
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
    QLabel* price[4] = {ui->label_dp_price1, ui->label_dp_price2, ui->label_dp_price3, ui->label_dp_price4};

    QCheckBox* check[4] = {ui->check1_DP, ui->check2_DP, ui->check3_DP, ui->check4_DP};

    for(int i = 0; i < 4; i++) {
        loadImageToLabel(pic[i], m_product_list[indexList[i]].imgPath); 
        name[i]->setText(m_product_list[indexList[i]].name);
        check[i]->setChecked(false);
        price[i]->setText(QString::number(m_product_list[indexList[i]].price) + " USD");
        price[i]->hide();
    }
    ui->rangeInp_DP->setText(QString("Given price: %1 USD").arg(m_product_list[indexList[m_index_DP]].price));
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

void MainWindow::setupTSC() {
    int id1 = randomProductIndex();
    int id2 = randomProductIndex();
    int id3 = randomProductIndex();
    while (id2 == id1) id2 = randomProductIndex();
    while (id3 == id1 || id3 == id2) id3 = randomProductIndex();

    int productIDs[3] = {id1, id2, id3};
    QLabel* picLabels[3] = {ui->label_show_case_pic1, ui->label_show_case_pic2, ui->label_show_case_pic3};
    QLabel* nameLabels[3] = {ui->label_show_case_name1, ui->label_show_case_name2, ui->label_show_case_name3};
    QLabel* priceLabels[3] = {ui->label_show_case_price1, ui->label_show_case_price2, ui->label_show_case_price3};

    for (int i = 0; i < 3; ++i) {
        loadImageToLabel(picLabels[i], m_product_list[productIDs[i]].imgPath);

        nameLabels[i]->setText(m_product_list[productIDs[i]].name);
        priceLabels[i]->setText(QString::number(m_product_list[productIDs[i]].price) + " USD");
        priceLabels[i]->hide();
    }

    if (m_product_list[id1].price > m_product_list[id2].price && m_product_list[id1].price > m_product_list[id3].price)
        m_solution_ME = 1;
    else if (m_product_list[id2].price > m_product_list[id1].price && m_product_list[id2].price > m_product_list[id3].price)
        m_solution_ME = 2;
    else
        m_solution_ME = 3;

    ui->totalPrice->setValue(0);
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
    m_score += bonus;
    ui->scoreInp->setText(QString::number(m_score));
    setupTSC();
}

void MainWindow::on_btnSubmitEndGame_clicked()
{
    ui->btnSubmitEndGame->setEnabled(false);

    int totalPrice = ui->totalPrice->text().toInt();
    int price1 = ui->label_show_case_price1->text().toInt();
    int price2 = ui->label_show_case_price2->text().toInt();
    int price3 = ui->label_show_case_price3->text().toInt();
    bonus = 0;

    if (qAbs(price1+price2+price3-totalPrice) <= 30) {
        QMessageBox::information(this, "RESULT", QString("Congratulations!!!"));
        ui->label_show_case_price1->show();
        ui->label_show_case_price2->show();
        ui->label_show_case_price3->show();

        if (m_p2p_mode) {
            m_p2p_score += 500;
        } else {
            m_score += 500;
            m_single_score += 500;
            ui->singleScore_SC->setText(QString::number(m_single_score) + " points");
        }
    } else {
        QMessageBox::information(this, "RESULT", QString("Your answer is wrong! Wish you luck next time!"));
        ui->label_show_case_price1->show();
        ui->label_show_case_price2->show();
        ui->label_show_case_price3->show();
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
        QMessageBox::information(this, "RESULT", QString("End game with score: %1").arg(m_single_score));
        m_single_score = 0;
        setCurrentTab(DISPLAY::MAIN);
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
    if (in_pvp == true) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Exit Game",
                                    "Do you want to exit the game?",
                                    QMessageBox::Yes|QMessageBox::No);
        
        if (reply == QMessageBox::Yes) {
            sendToServer("SUREN");
            setCurrentTab(DISPLAY::MAIN);
        } else return;
    } else {
        m_single_score = 0;
        setCurrentTab(DISPLAY::MAIN);
    }

}

void MainWindow::on_btnBackToMenu_GC_clicked() {
    if (in_pvp == true) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Exit Game",
                                    "Do you want to exit the game?",
                                    QMessageBox::Yes|QMessageBox::No);
        
        if (reply == QMessageBox::Yes) {
            sendToServer("SUREN");
            setCurrentTab(DISPLAY::MAIN);
        } else return;
    } else {
        m_single_score = 0;
        setCurrentTab(DISPLAY::MAIN);
    }
        
    
}

void MainWindow::on_btnBackToMenu_SP_clicked() {
    if (in_pvp == true) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Exit Game",
                                    "Do you want to exit the game?",
                                    QMessageBox::Yes|QMessageBox::No);
        
        if (reply == QMessageBox::Yes) {
            sendToServer("SUREN");
            setCurrentTab(DISPLAY::MAIN);
        } else return;
    } else  {
        m_single_score = 0;
        setCurrentTab(DISPLAY::MAIN);
    }
        
}

void MainWindow::on_btnBackToMenu_LW_clicked() {
    if (in_pvp == true) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Exit Game",
                                    "Do you want to exit the game?",
                                    QMessageBox::Yes|QMessageBox::No);
        
        if (reply == QMessageBox::Yes) {
            sendToServer("SUREN");
            setCurrentTab(DISPLAY::MAIN);
        } else return;
    } else {
        m_single_score = 0;
        setCurrentTab(DISPLAY::MAIN);}
}

void MainWindow::on_btnBackToMenu_SC_clicked()
{
    m_single_score = 0;
    setCurrentTab(DISPLAY::MAIN);
}

void MainWindow::on_btnBackToMenu_Room_clicked()
{   
    if(ui->player_name_2->text() == "...") {
        sendToServer("OUTRM" + m_acc_name);
        if(m_process_list.last() == PROCESS_MODE::GoRom)
            m_process_list.pop_back();
        setCurrentTab(DISPLAY::MAIN);
    } else {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Leave Room", 
                                    "Are you sure you want to leave the room?",
                                    QMessageBox::Yes|QMessageBox::No);
        
        if (reply == QMessageBox::Yes) {
            sendToServer("OUTRM" + m_acc_name);
            setCurrentTab(DISPLAY::MAIN);
        }
    }
}

void MainWindow::on_btnBackToMenu_Ranking_clicked()
{
    setCurrentTab(DISPLAY::MAIN);
}

void MainWindow::on_btnClear_clicked()
{
    ui->logConsole->clear();
}

void MainWindow::initOnlinePlayersTable() {
    ui->onlinePlayersTable->setColumnWidth(0, 120);
    ui->onlinePlayersTable->setColumnWidth(1, 80);
    ui->onlinePlayersTable->setColumnWidth(2, 80);
    ui->onlinePlayersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void MainWindow::updateOnlinePlayers(const QStringList& players) {
    // Clear existing rows
    ui->onlinePlayersTable->setRowCount(0);
    showLog("Online Players: " + players.join(", "));
    // Return if no players
    if (players.isEmpty()) {
        return;
    }

    // Add players if list not empty
    for(const QString& player : players) {
        int row = ui->onlinePlayersTable->rowCount();
        ui->onlinePlayersTable->insertRow(row);
        
        // Username
        QTableWidgetItem* nameItem = new QTableWidgetItem(player);
        ui->onlinePlayersTable->setItem(row, 0, nameItem);
        
        // Status 
        QTableWidgetItem* statusItem = new QTableWidgetItem("Online");
        ui->onlinePlayersTable->setItem(row, 1, statusItem);
        
         // Only add invite button if not self (first player)
        if (row > 0) {
            QPushButton* inviteBtn = new QPushButton("Invite");
            ui->onlinePlayersTable->setCellWidget(row, 2, inviteBtn);
            
            connect(inviteBtn, &QPushButton::clicked, this, [this, player]() {
                sendInvitation(player);
            });
        }
    }
}

void MainWindow::sendInvitation(const QString& player) {
    if (!m_has_room) {
        QMessageBox::warning(this, "Warning", 
            "Please create a room first by selecting a bet amount!");
        return;
    }
    
    qDebug() << "Sending invitation to:" << player;
    qDebug() << "Bet is:" << m_bet;
    sendToServer("INVIT" + m_acc_name + '|' + player + '|' + QString::number(m_bet));
}

void MainWindow::on_btnRefreshPlayers_clicked() {
    sendToServer("REFPL" + m_acc_name);
}