#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtNetwork>
#include <QTcpSocket>
#include <QObject>
#include <QByteArray>
#include <QDebug>
#include <QTcpServer>
#include <QTimer>


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE


struct Socket
{
    QTcpSocket* s_qtsSoket;
    int s_iPort;
    qint16 s_blockSize;
};


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_starting_clicked(); //Старт приложения
    void on_stoping_clicked();
    void newuser();

    void slotReadClient();
    void slotReadServer();

    void slotDisconectClient();
    void slotDisconnectedServer();

    void slotConnectedServer();
    void slotCloseNoConnectedServer();

    void slotSendPort();
private:
    Ui::MainWindow *ui;

    QTcpServer *tcpServer;
    int _iServerStatus;
    QString _qsAdressServer = "127.0.0.1";
    int _iPortServer = 6666;


    void _Synchronization(); //Синхронизация данных при первом включении
    void _DelServer();
    void _LoadHosts();
    QSet< QString > _set_firstAdressesServers;
    void _ConnectFirstHosts();
    bool flagNewConnectionServer;
    QTimer *timer;
    QSet< QString > _set_connectedAdressesServers;
    void _StartServer();



    QMap< int, Socket > _tmp_qm_sockServers;

    QMap< int, Socket > _qm_sockClients;
    QMap< int, Socket > _qm_sockServers;


};
#endif // MAINWINDOW_H
