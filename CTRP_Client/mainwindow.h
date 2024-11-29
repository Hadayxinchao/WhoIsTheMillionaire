
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QAbstractSocket>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QHostAddress>
#include <QMessageBox>
#include <QMetaType>
#include <QString>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QMutex>
#include <thread>
#include <QByteArray>
#include <QFile>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

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

class MainWindow : public QMainWindow

{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_lnIPAddress_textChanged(const QString &arg1);
    void on_btnConnect_clicked();
    void on_btnSignIn_clicked();
    void on_btnSingle_clicked();
    void on_btnPvP_clicked();
    void on_btnSubmitME_clicked();
    void on_btnSubmitG_clicked();
    void on_btnSubmitDP_clicked();
    void on_btnBET100_clicked();
    void on_btnBET200_clicked();
    void on_btnBET500_clicked();
    void on_btnStartPvP_clicked();
    void on_btnBackToMenu_clicked();
    void on_btnExitRoom_clicked();
    void on_btnBackToMenu2_clicked();
    void on_btnSubmitLW_clicked();
    void on_btnBackToMenu3_clicked();
    void on_btnRegister_clicked();
    void on_btnCancelRegister_clicked();
    void on_btnViewRank_clicked();
    void on_btnSpinWheel_clicked();
    void on_btnSubmitEndGame_clicked();
    void on_btnClear_clicked();

    void readSocketSlot();
    void discardSocket();
    void displayError(QAbstractSocket::SocketError socketError);
    void showLog(const QString& str);
    void sendToServer(QString str);
    void setCurrentTab(int index);
    void on_goRegister_clicked();
    void on_totalPrice_valueChanged(int arg1);

private:
    void setupME();
    void setupGrocery();
    void setupDP();
    void setupSpinWheel();
    void setupTSC();
    int randomProductIndex();
    void setupInRoom(int bet);

private:
    Ui::MainWindow *ui;
    QTcpSocket* m_socket;
    QString m_acc_name;
    int m_score;
    QList<int> m_process_list;
    QVector<Product> m_product_list;
    bool m_p2p_mode;
    int m_solution_ME;
    int m_total_G;
    int m_ID_G[5];
    int m_p2p_score;
    int m_bet;
    int m_index_DP;
    int bonus = 0;
    int secondBonus = 0;
};

enum PROCESS_MODE {
    LOGIN = 0,
    SIGNUP,
    ViewRank,
    GoRom,
    ReadyPvP,
    FinishPvP,

};

enum DISPLAY {
    MAIN = 0,
    MOST_EXPENSIVE,
    GROCERY,
    DANGER_PRICE,
    SPIN_WHEEL,
    SHOW_CASE,
    ROOM,
    SIGN_UP,
    VIEW_RANKING
};

#endif // MAINWINDOW_H
