#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    _iServerStatus = 0;
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_starting_clicked()
{
    _Synchronization();
}

void MainWindow::on_stoping_clicked()
{
    _DelServer();

    ui->textinfo->append(QString::fromUtf8("Сервер остановлен!"));
    qDebug() << QString::fromUtf8("Сервер остановлен!");
}


void MainWindow::newuser()
{
    if( _iServerStatus == 1 )
    {
        qDebug() << QString::fromUtf8( "У нас новое соединение!" );
        ui->textinfo->append( QString::fromUtf8( "У нас новое соединение!" ) );

        QTcpSocket* clientSocket = tcpServer->nextPendingConnection();

        qDebug() << "Client socket state = " << clientSocket->state();

        int id_user_socs=clientSocket->socketDescriptor();
        _qm_sockClients[ id_user_socs ] = { clientSocket, 0, 0 };

        connect( _qm_sockClients[ id_user_socs ].s_qtsSoket, SIGNAL( readyRead() ), this, SLOT( slotReadClient() ) );
        connect( _qm_sockClients[ id_user_socs ].s_qtsSoket, SIGNAL( disconnected() ), this, SLOT( slotDisconectClient() ) );

        QByteArray baOutData;
        QDataStream out( &baOutData, QIODevice::WriteOnly );

        out << (quint16)0;
        out << QString( "you_are_connected" );
        out.device()->seek(0);
        out << (quint16)(baOutData.size() - sizeof(quint16));

        clientSocket->write( baOutData );
    }
}

void MainWindow::slotReadClient()
{
    QTcpSocket* clientSocket = (QTcpSocket*)sender();
    int id_user_socs = clientSocket->socketDescriptor();

    QDataStream in( clientSocket );
    qint16 blz;

    if ( _qm_sockClients[ id_user_socs ].s_blockSize == 0) //если пришло меньше 2 байт ждем пока будет 2 байта
    {
        if (clientSocket->bytesAvailable() < (int)sizeof(quint16))
            return;

        in >> _qm_sockClients[ id_user_socs ].s_blockSize; //считываем размер (2 байта)
        blz = _qm_sockClients[ id_user_socs ].s_blockSize;
    }

    if (clientSocket->bytesAvailable() < _qm_sockClients[ id_user_socs ].s_blockSize) //ждем пока блок прийдет полностью
        return;
    else //можно принимать новый блок
        _qm_sockClients[ id_user_socs ].s_blockSize = 0;


    QString command;
    in >> command;

    ui->textinfo->append( "Клиент прислал команду: "+ command + " blocksize = " + QString::number( blz ) );

    if( command == "sync_adresses" )
    {
        qDebug( "Request sync" );

        QByteArray baOutData;
        QDataStream out( &baOutData, QIODevice::WriteOnly );

        out << (quint16)0;
        out << QString( "adresses_servers" );

        out << _qm_sockClients.size() - 1 + _qm_sockServers.size();

        QMap< int, Socket >::iterator it_qmap_i_qts;
        for( it_qmap_i_qts = _qm_sockClients.begin(); it_qmap_i_qts != _qm_sockClients.end(); ++it_qmap_i_qts )
        {
            if( it_qmap_i_qts.key() != id_user_socs && it_qmap_i_qts.value().s_iPort != 0 )
            {
                QHostAddress adripv4( it_qmap_i_qts.value().s_qtsSoket->peerAddress().toIPv4Address());
                QString adripv4_str = adripv4.toString();
                int port = it_qmap_i_qts.value().s_iPort;
                QString pname = it_qmap_i_qts.value().s_qtsSoket->peerName();
                out << adripv4_str << port;

                ui->textinfo->append( "Добавлен сервер в отправку клиенту Send " + adripv4_str + " " + QString::number( port ) );
            }
        }

        for( it_qmap_i_qts = _qm_sockServers.begin(); it_qmap_i_qts != _qm_sockServers.end(); ++it_qmap_i_qts )
        {
            out << QString( it_qmap_i_qts.value().s_qtsSoket->peerAddress().toString() ) << it_qmap_i_qts.value().s_iPort;
        }

        out.device()->seek(0);
        out << (quint16)(baOutData.size() - sizeof(quint16));
        clientSocket->write( baOutData );

        ui->textinfo->append( "Клиенту отправлены все текущие соединения" );
    }
    else if( command == "set_port" )
    {
        int port;
        in >> port;

        _qm_sockClients[ id_user_socs ].s_iPort = port;
    }
}


void MainWindow::_Synchronization()
{
    _DelServer();
    _LoadHosts();
    _ConnectFirstHosts();

    //QTimer::singleShot( 3000, this, SLOT( slotCloseNoConnectedServer() ) );
}

void MainWindow::_DelServer()
{
    if( _iServerStatus == 1 )
    {
        tcpServer->close();
        tcpServer->deleteLater();
        _iServerStatus = 0;
    }

    foreach( int i ,_qm_sockServers.keys() )
    {
        _qm_sockServers[i].s_qtsSoket->close();
        _qm_sockServers[i].s_qtsSoket->deleteLater();
        _qm_sockServers.remove( i );
    }

    foreach( int i ,_qm_sockClients.keys() )
    {
        _qm_sockClients[i].s_qtsSoket->close();
        _qm_sockClients[i].s_qtsSoket->deleteLater();
        _qm_sockClients.remove( i );
    }

    foreach( int i ,_tmp_qm_sockServers.keys() )
    {
        _tmp_qm_sockServers[i].s_qtsSoket->close();
        _tmp_qm_sockServers[i].s_qtsSoket->deleteLater();
        _tmp_qm_sockServers.remove( i );
    }

    _set_firstAdressesServers.clear();
    _set_connectedAdressesServers.clear();

    ui->textinfo->append( "Sucсess clear connection");
}

void MainWindow::_LoadHosts()
{
    QFile file( "Data/Addresses/AdressesServers.txt");
    QTextStream in( &file );

    if( file.open( QIODevice::ReadOnly ) )
    {
        while ( !in.atEnd() )
        {
            QString line = in.readLine();
            if( !line.isEmpty() )
            {
                _set_firstAdressesServers.insert( line );
            }
        }
        file.close();
    }
    else
    {
        ui->textinfo->append( "Error find file Data/Addresses/AdressesServers.txt");
    }
}

void MainWindow::_ConnectFirstHosts()
{
    if( _set_firstAdressesServers.size() != 0 )
    {
        for( const auto& setItem : _set_firstAdressesServers )
        {
            QTcpSocket* SockSyncServ = new QTcpSocket( this );
            connect( SockSyncServ, SIGNAL( connected() ), this, SLOT( slotConnectedServer() ) );

            QString adressSyncServ = setItem.mid(0, setItem.indexOf( ' ' ) );
            int portSyncServ = setItem.mid( setItem.indexOf( ' ' ) + 1 ).toInt();

            SockSyncServ->connectToHost( QHostAddress( adressSyncServ ), portSyncServ );

            qDebug() << "Состояние подключения: " << SockSyncServ->state();

            _tmp_qm_sockServers[ _tmp_qm_sockServers.size() ] = { SockSyncServ, portSyncServ, 0 };
        }
    }
    else
    {
        ui->textinfo->append( "No connection are available!");
    }

    timer = new QTimer( this );
    timer->setSingleShot( true );
    timer->setInterval( 3000 );
    connect( timer, SIGNAL( timeout() ), this, SLOT( slotCloseNoConnectedServer() ) );
    timer->start();
}

void MainWindow::slotConnectedServer()
{   
    timer->stop();

    QTcpSocket* serverSocket = (QTcpSocket*)sender();
    int id_serv_socs = serverSocket->socketDescriptor();

    ui->textinfo->append( "Удалось соединиться с узлом " + serverSocket->peerAddress().toString() + " " + QString::number( serverSocket->peerPort() ) );

    connect( serverSocket, SIGNAL( readyRead() ), this, SLOT( slotReadServer() ) );
    connect( serverSocket, SIGNAL( disconnected() ), this, SLOT( slotDisconnectedServer() ) );

    _qm_sockServers[ id_serv_socs ] = { serverSocket, serverSocket->peerPort(), 0 };
    _set_connectedAdressesServers.insert( QString( serverSocket->peerAddress().toString() + " " + QString::number( serverSocket->peerPort() ) ) );

    flagNewConnectionServer = true;

    timer = new QTimer( this );
    timer->setSingleShot( true );
    timer->setInterval( 3000 );
    connect( timer, SIGNAL( timeout() ), this, SLOT( slotCloseNoConnectedServer() ) );
    timer->start();
}

void MainWindow::slotCloseNoConnectedServer()
{
    _tmp_qm_sockServers.clear();
    _set_firstAdressesServers.clear();

    ui->textinfo->append( "Временные сервера в количестве: " + QString::number( _tmp_qm_sockServers.size() ) );
    ui->textinfo->append( "Текущие сервера в количестве: " + QString::number( _qm_sockServers.size() ) );

    if( _qm_sockServers.size() == 0 )
    {
        _StartServer();
    }
    else
    {
        if( flagNewConnectionServer )
        {
            flagNewConnectionServer = false;

            for( const auto& struct_server : _qm_sockServers )
            {
                QByteArray syn;
                QDataStream out( &syn, QIODevice::WriteOnly );

                out << (quint16)0;
                out << QString( "sync_adresses" );

                out.device()->seek(0);
                out << (quint16)(syn.size() - sizeof(quint16));

                struct_server.s_qtsSoket->write( syn );

                ui->textinfo->append( "Отправлено sync_adresses по адресу: " + struct_server.s_qtsSoket->peerAddress().toString() +
                                                " порт: " + QString::number( struct_server.s_qtsSoket->peerPort() )  );
            }

            timer = new QTimer( this );
            timer->setSingleShot( true );
            timer->setInterval( 3000 );
            connect( timer, SIGNAL( timeout() ), this, SLOT( slotCloseNoConnectedServer() ) );
            timer->start();
        }
        else
        {
            timer = new QTimer( this );
            timer->setSingleShot( true );
            timer->setInterval( 3000 );
            connect( timer, SIGNAL( timeout() ), this, SLOT( slotSendPort() ) );
            timer->start();
        }
    }
}

void MainWindow::slotReadServer()
{
    QTcpSocket* serverSocket = (QTcpSocket*)sender();
    int id_serv_socs = serverSocket->socketDescriptor();

    qDebug() << "socket descriptor in slotreadserv = " << serverSocket->socketDescriptor();
    QDataStream in( serverSocket );

    if ( _qm_sockServers[ id_serv_socs ].s_blockSize == 0 ) //если пришло меньше 2 байт ждем пока будет 2 байта
    {
        if ( serverSocket->bytesAvailable() < (int)sizeof( quint16 ) )
            return;

        in >> _qm_sockServers[ id_serv_socs ].s_blockSize; //считываем размер (2 байта)
    }

    if ( serverSocket->bytesAvailable() < _qm_sockServers[ id_serv_socs ].s_blockSize ) //ждем пока блок прийдет полностью
        return;
    else
        _qm_sockServers[ id_serv_socs ].s_blockSize = 0; //можно принимать новый блок

    QString command;
    in >> command;

    ui->textinfo->append( "Пришла от сервера команда: " + command );

    if( command == "adresses_servers" )
    {
        timer->stop();

        qDebug( "Считывание пошло");

        int size_servers;
        in >> size_servers;

        for( int i = 0; i < size_servers; ++i )
        {
            QString adr;
            int port;
            in >> adr >> port;

            QString checkAdressServer = QString( adr + " " + QString::number( port ) );
            if( _set_connectedAdressesServers.find( checkAdressServer ) == _set_connectedAdressesServers.end() )
            {
                QTcpSocket* server = new QTcpSocket( this );

                ui->textinfo->append("Прислан новый сервер: " + checkAdressServer );

                _set_connectedAdressesServers.insert( checkAdressServer );

                server->connectToHost( QHostAddress( adr ), port );
                connect( server, SIGNAL( connected() ), this, SLOT( slotConnectedServer() ) );
            }

            timer->stop();
            timer = new QTimer( this );
            timer->setSingleShot( true );
            timer->setInterval( 3000 );
            connect( timer, SIGNAL( timeout() ), this, SLOT( slotCloseNoConnectedServer() ) );
            timer->start();
        }

        timer->stop();
        timer = new QTimer( this );
        timer->setSingleShot( true );
        timer->setInterval( 3000 );
        connect( timer, SIGNAL( timeout() ), this, SLOT( slotCloseNoConnectedServer() ) );
        timer->start();
    }
}

void MainWindow::_StartServer()
{
    tcpServer = new QTcpServer(this);
    connect( tcpServer, SIGNAL( newConnection() ), this, SLOT( newuser() ) );

    if ( !tcpServer->listen(QHostAddress( _qsAdressServer ), _iPortServer ) && _iServerStatus == 0 )
    {
        qDebug() <<  QObject::tr("Unable to start the server: %1.").arg(tcpServer->errorString());
        ui->textinfo->append(tcpServer->errorString());
    }
    else
    {
        _iServerStatus = 1;
        qDebug() << tcpServer->isListening() << QString(" - TcpSocket listen on port " +  QString::number( _iPortServer ) );
        ui->textinfo->append(QString::fromUtf8("Сервер запущен!"));
        qDebug() << QString::fromUtf8("Сервер запущен!");
    }
}

void MainWindow::slotSendPort()
{
    _StartServer();

    for( const auto& server : _qm_sockServers )
    {
        QByteArray block_port;
        QDataStream out( &block_port, QIODevice::WriteOnly );

        out << (quint16)0;
        out << QString( "set_port" );
        out << _iPortServer;

        out.device()->seek(0);
        out << (quint16)(block_port.size() - sizeof(quint16));

        server.s_qtsSoket->write( block_port );

        ui->textinfo->append( "Отправлен свой порт " + QString::number( _iPortServer ) + " по адресу: " + server.s_qtsSoket->peerAddress().toString() +
                                        " порт: " + QString::number( server.s_qtsSoket->peerPort() )  );
    }
}

void MainWindow::slotDisconectClient()
{
    QMap< int, Socket >::iterator del_tcp_it;
    for( del_tcp_it = _qm_sockClients.begin(); del_tcp_it != _qm_sockClients.end(); ++del_tcp_it )
    {
        if( del_tcp_it.value().s_qtsSoket->state() == QAbstractSocket::UnconnectedState )
        {
            ui->textinfo->append( "Удален клиент: " + del_tcp_it.value().s_qtsSoket->peerAddress().toString() + " " + QString::number( del_tcp_it.value().s_iPort ) );
            del_tcp_it.value().s_qtsSoket->close();
            del_tcp_it.value().s_qtsSoket->deleteLater();
            _qm_sockClients.remove( del_tcp_it.key() );
            break;
        }
    }
    qDebug() << QString::fromUtf8( "Disconnect" );
}


void MainWindow::slotDisconnectedServer()
{
    ui->textinfo->append( "Сервер на той стороне отключился" );

    QMap< int, Socket >::iterator del_tcp_it;
    for( del_tcp_it = _qm_sockServers.begin(); del_tcp_it != _qm_sockServers.end(); ++del_tcp_it )
    {
        if( del_tcp_it.value().s_qtsSoket->state() == QAbstractSocket::UnconnectedState )
        {
            ui->textinfo->append( "Найден отключенный сервер " + del_tcp_it.value().s_qtsSoket->peerAddress().toString() + " " + QString::number( del_tcp_it.value().s_qtsSoket->peerPort() ) + " " + QString::number( del_tcp_it.value().s_iPort ) ) ;
            del_tcp_it.value().s_qtsSoket->close();
            del_tcp_it.value().s_qtsSoket->deleteLater();
            _qm_sockServers.remove( del_tcp_it.key() );
            break;
        }
    }

    qDebug() << QString::fromUtf8( "Disconnect server" );
}
