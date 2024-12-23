#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QMetaType>
#include <QSet>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QList>
#include <algorithm>
#include "account.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

struct RoomInfo;

struct ClientInfo {
    QTcpSocket* sockfd;
    Account* acc;
    RoomInfo *room;

    ClientInfo(QTcpSocket* sock) {
        sockfd = sock;
        acc = nullptr;
        room = nullptr;
    }
};

struct Product
{
    QString name;
    int price;
    QString imgPath;
    Product(QString _n, int _p, QString _path) {
        name = _n;
        price = _p;
        imgPath = _path;
    }
};

struct RoomInfo
{
    int type;
    ClientInfo *p1;
    ClientInfo *p2;
    int p1_status;
    int p2_status;
    RoomInfo(int _t) {
        type = _t;
        p1 = nullptr;
        p2 = nullptr;
        p1_status = -2;
        p2_status = -2;
    }
};

class MainWindow : public QMainWindow

{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
//    void newMessage(QString);
private slots:
    void newConnection();
    void appendToSocketList(QTcpSocket* socket);

    void readSocket();
    void discardSocket();
    void displayError(QAbstractSocket::SocketError socketError);

    void sendMessage(QTcpSocket* socket, QString str);
private:
    void showLog(QString msg);
    ClientInfo* findClient(QTcpSocket* sock, int &i);
    void processLogIn(QTcpSocket *socket, ClientInfo *client, QString &data);
    void processSignUp(QTcpSocket *socket, ClientInfo *client, QString &data);
    void processViewRank(QTcpSocket *socket);
    void processViewCurrentPlayers(QTcpSocket *socket);
private:
    Ui::MainWindow *ui;

    QTcpServer* m_server;
    QVector<ClientInfo*> m_client_set;
    QVector<Account*> m_acc_set;
//    QVector<Product> m_product_list;
    QList<RoomInfo*> m_room_list[3];
};

#endif // MAINWINDOW_H
