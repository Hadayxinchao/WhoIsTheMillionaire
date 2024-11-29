
#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <QFile>
#include <QTextStream>
#include <QDebug>

#define ACC_FILE_NAME "./accounts.txt"

class Account
{
public:
    QString id;
    QString pwd;
    unsigned int score;
    bool loginStatus;

public:
    Account(QString _id, QString _pwd, QString _s) {
        id = _id;
        pwd = _pwd;
        score = _s.toInt();
        loginStatus = false;
    }

    QString toString() {
        return (id + " " + pwd + " " + QString::number(score));
    }
};

#endif // ACCOUNT_H
