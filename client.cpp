#include "client.h"

Client::Client(QWidget *parent) : 
    QDialog(new QDialog),
    hostCombo(new QComboBox),
    portLineEdit(new QLineEdit),
    getWallsButton(new QPushButton(tr("Get walls"))),
    tcpSocket(new QTcpSocket(this))
{
    hostCombo->setEditable(true);
    QString name = QHostInfo::localHostName();
    if (!name.isEmpty()) {
        hostCombo->addItem(name);
        QString domain = QHostInfo::localDomainName();
        if (!domain.isEmpty())
            hostCombo->addItem(name + QChar('.') + domain);
    }
    
    if (name != QLatin1String("localhost"))
        hostCombo->addItem(QString("localhost"));
    
    QList<QHostAddress> ipAddressesList = QNetworkInterface::allAddresses();
    for (int i = 0; i < ipAddressesList.size(); i++){
        if(!ipAddressesList.at(i).isLoopback())    
            hostCombo->addItem(ipAddressesList.at(i).toString());
    }
    for (int i = 0; i < ipAddressesList.size(); i++){
        if(ipAddressesList.at(i).isLoopback())    
            hostCombo->addItem(ipAddressesList.at(i).toString());
    }
    
    portLineEdit->setValidator(new QIntValidator(1, 65535, this));
    
    auto hostLabel = new QLabel(tr("&Server name:"));
    hostLabel->setBuddy(hostCombo);
    auto portLabel = new QLabel(tr("S&erver port:"));
    portLabel->setBuddy(portLineEdit);
    
    statusLabel = new QLabel(tr("Status label"));
    
    getWallsButton->setDefault(true);
    getWallsButton->setEnabled(false);
    
    auto quitButton = new QPushButton(tr("Quit"));
    
    auto buttonBox = new QDialogButtonBox;
    buttonBox->addButton(getWallsButton, QDialogButtonBox::ActionRole);
    buttonBox->addButton(quitButton, QDialogButtonBox::RejectRole);
    
    in.setDevice(tcpSocket);
    in.setVersion(QDataStream::Qt_4_0);
    
    connect(hostCombo, SIGNAL(editTextChanged(QString)),
            this, SLOT(enableGetWallsButton()));
    connect(portLineEdit, SIGNAL(textChanged(QString)),
            this, SLOT(enableGetWallsButton()));
    connect(getWallsButton, SIGNAL(clicked(bool)),
            this, SLOT(requestWalls()));
    connect(quitButton, SIGNAL(clicked(bool)),
            this, SLOT(close()));
    connect(tcpSocket, SIGNAL(readyRead()),
            this, SLOT(readData()));
    connect(tcpSocket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(displayError()));
    
    
    QGridLayout *mainLayout = nullptr;
    if (QGuiApplication::styleHints()->showIsFullScreen() || QGuiApplication::styleHints()->showIsMaximized()) {
        auto outerVerticalLayout = new QVBoxLayout(this);
        outerVerticalLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Ignored, QSizePolicy::MinimumExpanding));
        auto outerHorizontalLayout = new QHBoxLayout;
        outerHorizontalLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::MinimumExpanding, QSizePolicy::Ignored));
        auto groupBox = new QGroupBox(QGuiApplication::applicationDisplayName());
        mainLayout = new QGridLayout(groupBox);
        outerHorizontalLayout->addWidget(groupBox);
        outerHorizontalLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::MinimumExpanding, QSizePolicy::Ignored));
        outerVerticalLayout->addLayout(outerHorizontalLayout);
        outerVerticalLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Ignored, QSizePolicy::MinimumExpanding));
    } else {
        mainLayout = new QGridLayout(this);
    }
    
    mainLayout->addWidget(hostLabel, 0, 0);
    mainLayout->addWidget(hostCombo, 0, 1);
    mainLayout->addWidget(portLabel, 1, 0);
    mainLayout->addWidget(portLineEdit, 1, 1);
    mainLayout->addWidget(statusLabel, 2, 0, 1, 2);
    mainLayout->addWidget(buttonBox, 3, 0, 1, 2);

    setWindowTitle(QGuiApplication::applicationDisplayName());
    portLineEdit->setFocus();
    
    QNetworkConfigurationManager manager;
    if (manager.capabilities() & QNetworkConfigurationManager::NetworkSessionRequired) {
        
        QSettings settings(QSettings::UserScope, QLatin1String("QtProject"));
        settings.beginGroup(QLatin1String("QtNetwork"));
        const QString id = settings.value(QLatin1String("DefaultNetworkConfiguration")).toString();
        settings.endGroup();
        
        QNetworkConfiguration config = manager.configurationFromIdentifier(id);
        if ((config.state() & QNetworkConfiguration::Discovered) !=
            QNetworkConfiguration::Discovered) {
            config = manager.defaultConfiguration();
        }
        networkSession = new QNetworkSession(config, this);
        connect(networkSession, &QNetworkSession::opened, this, &Client::sessionOpened);
        getWallsButton->setEnabled(false);
        statusLabel->setText(tr("Opening network session."));
        networkSession->open();
    }
}



void Client::requestWalls(){
    getWallsButton->setEnabled(true);
    tcpSocket->abort();
    tcpSocket->connectToHost(hostCombo->currentText(),
                             portLineEdit->text().toInt());
}

void Client::readData(){
    in.startTransaction();
    QString data;
    
    in >> data;
    qDebug() << data;
    
    if(data.split(".").at(0) == "b"){
        
        QTimer::singleShot(100, this, SLOT(transBall()));
    }
    if(data.split(".").at(0) == "w"){
        QString wall = data;
        
        int amount = wall.split(".").at(1).toInt(); 
        wall.remove(0,4);
        for (int i = 0; i < amount; i++)
            allWalls.push_back(wall.split("=")[i]);
        for (int i = 0; i < amount; i++)
            qDebug() << allWalls[i];   
        QTimer::singleShot(100, this, SLOT(transWall()));
    }
    
    if(!in.commitTransaction())
        return;
    
    currentData = data;
    statusLabel->setText(currentData);
    getWallsButton->setEnabled(true);
}

void Client::displayError(QAbstractSocket::SocketError socketError)
{
    switch (socketError) {
    case QAbstractSocket::RemoteHostClosedError:
        break;
    case QAbstractSocket::HostNotFoundError:
        QMessageBox::information(this, tr("Ball client"),
                                 tr("The host was not found. Please check the "
                                    "host name and port settings."));
        break;
    case QAbstractSocket::ConnectionRefusedError:
        QMessageBox::information(this, tr("Ball Client"),
                                 tr("The connection was refused by the peer. "
                                    "Make sure the ball server is running, "
                                    "and check that the host name and port "
                                    "settings are correct."));
        break;
    default:
        QMessageBox::information(this, tr("Ball Client"),
                                 tr("The following error occurred: %1.").arg(tcpSocket->errorString()));
    }

    getWallsButton->setEnabled(true);
}

void Client::enableGetWallsButton(){
    getWallsButton->setEnabled((!networkSession || networkSession->isOpen()) &&
                               !hostCombo->currentText().isEmpty() &&
                               !portLineEdit->text().isEmpty());
}

void Client::sessionOpened(){
    QNetworkConfiguration config = networkSession->configuration();
    QString id;
    if(config.type() == QNetworkConfiguration::UserChoice)
        id = networkSession->sessionProperty(QLatin1String("UserChoiceConfiguration")).toString();
    else id = config.identifier();
    
    QSettings settings(QSettings::UserScope, QLatin1String("QtProject"));
    settings.beginGroup(QLatin1String("QtNetwork"));
    settings.setValue(QLatin1String("DefaultNetworkConfiguration"), id);
    settings.endGroup();

    statusLabel->setText(tr("This huita requires that you run the "
                            "Ball Server blyat as well."));

    enableGetWallsButton();
}


QVector<QString> Client::throwWalls(){
    return this->allWalls;
}

void Client::transWall(){
    emit got_wall();    
}

void Client::transBall(){
    emit got_ball();    
}



void Client::sendBall(Ball b){
    tcpSocket->connectToHost(hostCombo->currentText(),
                             portLineEdit->text().toInt());
    
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_4_0);
    
    QString ball = QString::number(b.GetCoord().x()) + ":" +
                   QString::number(b.GetCoord().y()) + ":" + 
                   QString::number(b.getSpeed().x()) + ":" +
                   QString::number(b.getSpeed().y()) + ":" + 
                   QString::number(b.getAccel().x()) + ":" +
                   QString::number(b.getAccel().y());
    out << ball;
    
    //connect(serverConnection, SIGNAL(disconnected()),
    //        serverConnection, SLOT(deleteLater()));
    
    tcpSocket->write(block);
}


