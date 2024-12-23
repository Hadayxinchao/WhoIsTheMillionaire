#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_server = new QTcpServer();

    if(m_server->listen(QHostAddress::Any, 8080))
    {
        connect(m_server, &QTcpServer::newConnection, this, &MainWindow::newConnection);
        showLog("Server is listening...");
    }
    else
    {
        QMessageBox::critical(this,"SERVER", QString("Unable to start the server: %1.").arg(m_server->errorString()));
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

//    // Read product list:
//    QFile file1(":/resources/Products/products_list.txt");
//    if (!file1.open(QFile::ReadOnly | QFile::Text))
//    {
//        QMessageBox::critical(this, "SERVER",QString("Cannot open products_list.txt"));
//        exit(EXIT_FAILURE);
//    }
//    m_product_list.push_back(Product("none", 0, "image_none.jpg"));
//    QTextStream in1(&file);
//    QString _name, _price, _path, _id;
//    while (!in1.atEnd())
//    {
//        _id = in1.readLine();
//        _name = in1.readLine();
//        _price = in1.readLine();
//        _path = in1.readLine();
//        if(_id.isEmpty() || _name.isEmpty() || _price.isEmpty() || _path.isEmpty())
//            break;
//        m_product_list.push_back(Product(_name, _price.toInt(), _path));
//        if (in1.status() == QTextStream::ReadCorruptData)
//            break;
//    }
//    file1.close();
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
        if(m_client_set[i]->sockfd== sock)
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

void MainWindow::processViewCurrentPlayers(QTcpSocket* socket)
{
    // Make a copy of the accounts set
    QVector<Account*> sorted_acc_set = m_acc_set;

    // Sort the accounts in descending order of scores
    std::sort(sorted_acc_set.begin(), sorted_acc_set.end(), [](Account* a, Account* b){
        return a->score > b->score;
    });

    QString onlineList;

    // Prepare the string containing top 10 or fewer users
    for(int i = 0; i < qMin(10, sorted_acc_set.size()); i++)
    {   
        if (sorted_acc_set[i]->loginStatus == true) {
            onlineList += sorted_acc_set[i]->id + "|";
        }
    }

    // Send the current online players list back to the client
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
        if(data == "100")
            type = 0;
        else if(data == "200")
            type = 1;
        else if(data == "500")
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
            showLog(room->p1->acc->id + " out of room");
            m_room_list[room->type].removeAll(room);
            delete room;
        }
        client->room = nullptr;
    }

    else if(header == "ENDGA") {
        showLog(QString("%1 :: Save score with account ").arg(socket->socketDescriptor()) + client->acc->id);
        client->acc->score = data.toInt();
    }

    else if(header == "REFPL") {
        processViewCurrentPlayers(socket);
    }

    else if(header == "INVIT") {
        ClientInfo *invited = nullptr;
        for(int i = 0; i < m_client_set.size(); i++) {
            if(m_client_set[i]->acc != nullptr && m_client_set[i]->acc->id == data) {
                invited = m_client_set[i];
                break;
            }
        }
        if(invited != nullptr) {
            sendMessage(invited->sockfd, "INVIT" + client->acc->id);
        }
    }

    else if(header == "ACCRE") {
        Account *acc = nullptr;
        for(int i = 0; i < m_acc_set.size(); i++) {
            
            if(m_acc_set[i]->id == data) {
                acc = m_acc_set[i];
                break;
            }
        }
        if(acc != nullptr) {
            sendMessage(socket, "ACCRE" + acc->id + "|" + acc->pwd + "|" + QString::number(acc->score));
        }
    }

    else if(header == "SPWHE") {
        int score = data.toInt();
        client->acc->score += score;
        sendMessage(socket, "SPWHE" + QString::number(client->acc->score));
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
