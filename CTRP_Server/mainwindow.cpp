#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QNetworkInterface>
#include <QHostAddress>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_server = new QTcpServer();

    if (m_server->listen(QHostAddress::Any, 8080)) {
        connect(m_server, &QTcpServer::newConnection, this, &MainWindow::newConnection);

        QString ipAddress;
        for (const QNetworkInterface &interface : QNetworkInterface::allInterfaces()) {
            if (interface.flags().testFlag(QNetworkInterface::IsUp) &&
                interface.flags().testFlag(QNetworkInterface::IsRunning) &&
                !interface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
                
                for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
                    if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                        ipAddress = entry.ip().toString();
                        break;
                    }
                }
            }
            if (!ipAddress.isEmpty()) break;
        }

        if (ipAddress.isEmpty()) {
            ipAddress = QHostAddress(QHostAddress::LocalHost).toString();
        }

        showLog(QString("Server is listening on IP: %1, Port: 8080").arg(ipAddress));
    } 
    else {
        QMessageBox::critical(this, "SERVER", 
            QString("Unable to start the server: %1.").arg(m_server->errorString()));
        exit(EXIT_FAILURE);
    }

    // read account list
    QFile file(ACC_FILE_NAME);
    if (!file.open(QFile::ReadOnly | QFile::Text))
    {
        QMessageBox::critical(this, "SERVER", QString("Cannot open " + QString(ACC_FILE_NAME)));
        exit(EXIT_FAILURE);
    }

    QTextStream in(&file);
    while (!in.atEnd())
    {
        QStringList lstr = in.readLine().split(" ");
        if(lstr.size() != 3 || lstr[0].isEmpty() || lstr[1].isEmpty() || lstr[2].isEmpty())
            break;
        m_acc_set.push_back(new Account(lstr[0], lstr[1], lstr[2]));
        if (in.status() == QTextStream::ReadCorruptData)
            break;
    }
    file.close();
}

MainWindow::~MainWindow()
{
    QFile file(ACC_FILE_NAME);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        foreach (Account* acc, m_acc_set) {
            stream << acc->toString() << "\n";
            delete acc;
        }
        file.close();
    }
    else QMessageBox::critical(this,"SERVER", QString("Cannot create accounts.txt"));

    for (int i = 0; i < 3; i++) {
        foreach (RoomInfo *var, m_room_list[i]) {
            delete var;
        }
    }

    foreach (ClientInfo* cli, m_client_set) {
        cli->sockfd->close();
        cli->sockfd->deleteLater();
        delete cli;
    }

    m_server->close();
    m_server->deleteLater();

    delete ui;
}

void MainWindow::newConnection()
{
    while (m_server->hasPendingConnections())
        appendToSocketList(m_server->nextPendingConnection());
}

void MainWindow::appendToSocketList(QTcpSocket* socket)
{
    m_client_set.push_back(new ClientInfo(socket));
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::readSocket);
    connect(socket, &QTcpSocket::disconnected, this, &MainWindow::discardSocket);
    connect(socket, &QAbstractSocket::errorOccurred, this, &MainWindow::displayError);
    showLog(QString("-> Client with sockd:%1 has just connected").arg(socket->socketDescriptor()));
}

ClientInfo* MainWindow::findClient(QTcpSocket *sock, int &i) {
    for(i = 0; i < m_client_set.size(); i++){
        if(m_client_set[i]->sockfd == sock)
            return m_client_set[i];
    }
    return nullptr;
}

void MainWindow::processLogIn(QTcpSocket *socket, ClientInfo *client, QString &data) {
    QStringList lstr = data.split('|');
    Account *acc = nullptr;
    for(int i = 0; i < m_acc_set.size(); i++) {
        if(m_acc_set[i]->id == lstr[0] && m_acc_set[i]->pwd == lstr[1]) {
            acc = m_acc_set[i];
            break;
        }
    }
    if(acc == nullptr) {
        sendMessage(socket, "LOGF1: ID or password is not correct.");
        return;
    }
    if(acc->loginStatus) {
        sendMessage(socket, "LOGF2: Account has login on other client.");
        return;
    }
    client->acc = acc;
    acc->loginStatus = true;
    sendMessage(socket, "SUCLI: " + QString::number(acc->score));
    showLog(QString("%1 :: Login with account ").arg(socket->socketDescriptor()) + acc->id);
    return;
}

void MainWindow::processSignUp(QTcpSocket* socket, ClientInfo* client, QString& data)
{
    QStringList lstr = data.split('|');
    Account* acc = nullptr;

    if(lstr[1] != lstr[2])
    {
        sendMessage(socket, "CFPNM");
        return;
    }

    for (int i = 0; i < m_acc_set.size(); i++)
    {
        if (m_acc_set[i]->id == lstr[0])
        {
            sendMessage(socket, "Username already exists.");
            return;
        }
    }
    // New user will have 10000 score
    acc = new Account(lstr[0], lstr[1], "10000");
    m_acc_set.push_back(acc);
    sendMessage(socket, "SIGNUP SUCCESS");
    showLog(QString("New account created: %1").arg(acc->id));
    return;
}

void MainWindow::processViewRank(QTcpSocket* socket)
{
    // Make a copy of the accounts set
    QVector<Account*> sorted_acc_set = m_acc_set;

    // Sort the accounts in descending order of scores
    std::sort(sorted_acc_set.begin(), sorted_acc_set.end(), [](Account* a, Account* b){
        return a->score > b->score;
    });

    QString rankList;

    // Prepare the string containing top 10 or fewer users
    for(int i = 0; i < qMin(10, sorted_acc_set.size()); i++)
    {
        rankList += sorted_acc_set[i]->id + "|" + QString::number(sorted_acc_set[i]->score) + "|";
    }

    // Send the ranking list back to the client
    sendMessage(socket, rankList);
}

void MainWindow::processViewCurrentPlayers(QTcpSocket* socket, QString current_player_id)
{
    QVector<Account*> sorted_acc_set = m_acc_set;

    std::sort(sorted_acc_set.begin(), sorted_acc_set.end(), [](Account* a, Account* b){
        return a->score > b->score;
    });

    QString onlineList = "REFPL" + current_player_id + "|";

    for(int i = 0; i < qMin(10, sorted_acc_set.size()); i++)
    {   
        if (sorted_acc_set[i]->loginStatus == true && sorted_acc_set[i]->id != current_player_id) {
            onlineList += sorted_acc_set[i]->id + "|";
        }
    }
    
    // Remove last separator if exists
    if (onlineList.endsWith('|')) {
        onlineList.chop(1);
    }
    
    showLog(onlineList);
    sendMessage(socket, onlineList);
}

void MainWindow::readSocket()
{
    QTcpSocket* socket = reinterpret_cast<QTcpSocket*>(sender());

    QByteArray buffer;

    QDataStream socketStream(socket);
    socketStream.setVersion(QDataStream::Qt_5_15);

    socketStream.startTransaction();
    socketStream >> buffer;

    if(!socketStream.commitTransaction())
    {
        QString message = QString("%1 :: Waiting for more data to come..").arg(socket->socketDescriptor());
        showLog(message);
        return;
    }
    int clientInd;
    ClientInfo *client = findClient(socket, clientInd);
    if(client == nullptr)
        return;
    QString message = QString::fromStdString(buffer.toStdString());
    showLog(QString::number(socket->socketDescriptor()) + ": " + message);
    QString header = message.left(5);
    QString data = message.mid(5);

    if(header == "LOGIN") {
        processLogIn(socket, client, data);
    }

    else if(header == "SIGNU") {
        processSignUp(socket, client, data);
    }

    else if(header == "LOGOU") {
        showLog(QString("%1 :: Logout with account ").arg(socket->socketDescriptor()) + client->acc->id);
        client->acc->score = data.toInt();
        client->acc->loginStatus = false;
        client->acc = nullptr;
    }

    else if(header == "VIEWR") {
        processViewRank(socket);
    }

    else if(header == "GROOM") {
        int type;
        if(data == "1000")
            type = 0;
        else if(data == "2000")
            type = 1;
        else if(data == "5000")
            type = 2;
        else
            return;
        if(m_room_list[type].size() == 0 || m_room_list[type].last()->p2 != nullptr) {
            RoomInfo *newRoom = new RoomInfo(type);
            newRoom->p1 = client;
            client->room = newRoom;
            m_room_list[type].push_back(newRoom);
            showLog(client->acc->id + " is waiting in room " + data);
        }
        else {
            RoomInfo *room = m_room_list[type].last();
            room->p2 = client;
            client->room = room;
            sendMessage(room->p1->sockfd, "READY" + client->acc->id + "|" + QString::number(client->acc->score));
            sendMessage(socket, "READY" + room->p1->acc->id + "|" + QString::number(room->p1->acc->score));
            showLog(room->p1->acc->id + " vs " + client->acc->id + " in room " + data);
        }
    }

    else if(header == "READY") {
        RoomInfo *room = client->room;
        if(room->p1 == client)
            room->p1_status = -1;
        else
            room->p2_status = -1;
        if(room->p1_status == -1 && room->p2_status == -1) {
            sendMessage(room->p1->sockfd, "START_PVP");
            sendMessage(room->p2->sockfd, "START_PVP");
        }
    }

    else if(header == "RSPVP") {
        int score = data.toInt();
        RoomInfo *room = client->room;
        if(room->p1 == client)
            room->p1_status = score;
        else
            room->p2_status = score;
        if(room->p1_status > -1 && room->p2_status > -1) {
            sendMessage(room->p1->sockfd, "RSPVP" + QString::number(room->p2_status));
            sendMessage(room->p2->sockfd, "RSPVP" + QString::number(room->p1_status));
            room->p1->room = nullptr;
            room->p2->room = nullptr;
            m_room_list[room->type].removeAll(room);
            delete room;
        }
//        showLog(QString(QString::number(data.toInt())+":"+QString::number(room->p2_status)+":"+QString::number(room->p1_status)));
        client->acc->score += data.toInt();
    }

    else if(header == "OUTRM") {
    RoomInfo *room = client->room;
    if(room != nullptr) {
        // Player 1 leaving
        if(room->p1 == client) {
            if(room->p2 != nullptr) {
                sendMessage(room->p2->sockfd, "OUTRM" + client->acc->id);
            }
            room->p1 = nullptr;
        }
        // Player 2 leaving  
        else if(room->p2 == client) {
            if(room->p1 != nullptr) {
                sendMessage(room->p1->sockfd, "OUTRM" + client->acc->id);
            }
            room->p2 = nullptr;
        }

        showLog(client->acc->id + " out of room");
        client->room = nullptr;

        // Only delete room if both players gone
        if(room->p1 == nullptr && room->p2 == nullptr) {
            m_room_list[room->type].removeAll(room);
            delete room;
        }
    }
}

    else if(header == "ENDGA") {
        showLog(QString("%1 :: Save score with account ").arg(socket->socketDescriptor()) + client->acc->id);
        client->acc->score = data.toInt();
    }

    else if(header == "REFPL") {
        processViewCurrentPlayers(socket, data);
    }

    else if(header == "INVIT") {
        QString inviter = data.split('|')[0];
        QString invited = data.split('|')[1];
        int m_bet = data.split('|')[2].toInt();
        ClientInfo *invited_player = nullptr;
        for(int i = 0; i < m_client_set.size(); i++) {
            if(m_client_set[i]->acc != nullptr && m_client_set[i]->acc->id == invited) {
                invited_player = m_client_set[i];
                break;
            }
        }
        if(invited_player != nullptr) {
            sendMessage(invited_player->sockfd, "INVIT" + client->acc->id + "|" + QString::number(m_bet));
        }
    }

    else if(header == "SUREN") {
        RoomInfo *room = client->room;
        if(room != nullptr) {
            int bet = 0;
            if (room->type == '0') {
                bet = 1000;
            } else if (room->type == '1') {
                bet = 2000;
            } else if (room->type == '2') {
                bet = 5000;
            }
            // Player who surrendered loses
            if(room->p1 == client) {
                sendMessage(room->p1->sockfd, "SURLS"); // Surrender lose
                sendMessage(room->p2->sockfd, "SURWN"); // Surrender win
                room->p2->acc->score += bet;
                room->p1->acc->score -= bet;
            } else {
                sendMessage(room->p1->sockfd, "SURWN"); 
                sendMessage(room->p2->sockfd, "SURLS");
                room->p1->acc->score += bet;
                room->p2->acc->score -= bet;
            }
            
            // Clean up room
            room->p1->room = nullptr;
            room->p2->room = nullptr;
            m_room_list[room->type].removeAll(room);
            delete room;
        }
    }
}

void MainWindow::discardSocket()
{
    QTcpSocket* socket = reinterpret_cast<QTcpSocket*>(sender());
    int index;
    ClientInfo* it = findClient(socket, index);
    if (it != nullptr) {
        showLog(QString("Client number %1 :: Disconnected").arg(index));
        if(it->acc != nullptr)
            it->acc->loginStatus = false;
        m_client_set.removeAt(index);
    }

    socket->deleteLater();
}

void MainWindow::displayError(QAbstractSocket::SocketError socketError)
{
    switch (socketError) {
    case QAbstractSocket::RemoteHostClosedError:
        break;
    case QAbstractSocket::HostNotFoundError:
        QMessageBox::information(this, "QTCPServer", "The host was not found. Please check the host name and port settings.");
        break;
    case QAbstractSocket::ConnectionRefusedError:
        QMessageBox::information(this, "QTCPServer", "The connection was refused by the peer. Make sure QTCPServer is running, and check that the host name and port settings are correct.");
        break;
    default:
        QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
        QMessageBox::information(this, "QTCPServer", QString("The following error occurred: %1.").arg(socket->errorString()));
        break;
    }
}

void MainWindow::sendMessage(QTcpSocket* socket, QString str)
{
    if(socket)
    {
        if(socket->isOpen())
        {
            QDataStream socketStream(socket);
            QByteArray byteArray = str.toUtf8();

            socketStream.setVersion(QDataStream::Qt_5_15);
            socketStream << byteArray;
            socket->waitForBytesWritten();
        }
        else
            showLog(QString("Socket %1 doesn't seem to be opened").arg(socket->socketDescriptor()));
    }
    else
        showLog(QString("Socket Not connected"));
}

void MainWindow::showLog(QString msg) {
    ui->logConsole->append(msg);
    qDebug() << msg;
}
