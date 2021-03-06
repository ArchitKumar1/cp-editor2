#include <CompanionServer.hpp>
#include <MessageLogger.hpp>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

namespace Network
{
CompanionServer::CompanionServer(int port)
{
    server = new QTcpServer(this);
    portNumber = port;
    server->setMaxPendingConnections(1);
    QObject::connect(server, SIGNAL(newConnection()), this, SLOT(onNewConnection()));
    server->listen(QHostAddress::LocalHost, static_cast<unsigned short>(port));

    if (server->serverPort() == 0)
        Log::MessageLogger::error("Companion", "Failed to listen to " + std::to_string(port) +
                                                   " port. Is another process listening there?");
    else
        Log::MessageLogger::info("Companion", "Server is listening on " +
                                                  server->serverAddress().toString().toStdString() + ":" +
                                                  std::to_string(server->serverPort()));
}

CompanionServer::~CompanionServer()
{
    Log::MessageLogger::info("Companion", "Stopped Server");
    delete server;
}

void CompanionServer::onNewConnection()
{
    qDebug() << "New Connection request arrived\n";
    socket = server->nextPendingConnection();
    QObject::connect(socket, SIGNAL(readyRead()), this, SLOT(onReadReady()));
    QObject::connect(socket, SIGNAL(disconnected()), this, SLOT(onTerminateConnection()));
}

void CompanionServer::onReadReady()
{
    QString request = socket->readAll();
    if (request.startsWith("POST") && request.contains("Content-Type: application/json"))
    {
        Log::MessageLogger::info("Companion", "Got a POST Request");

        socket->write("HTTP/1.1  OK\r\n"); // \r needs to be before \n
        socket->write("Content-Type: text/html\r\n");
        socket->write("Connection: close\r\n");
        socket->write("Pragma: no-cache\r\n");
        socket->write("\r\n");
        socket->write("<!DOCTYPE html>\r\n");
        socket->write("<html><body><h1>Okay, Accepted"
                      "</h1></body></html>");
        socket->write(QByteArray());
        socket->disconnectFromHost();

        request = request.mid(request.indexOf('{'));

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(request.toUtf8(), &error);
        if (error.error == QJsonParseError::NoError)
        {
            CompanionData payload;

            payload.name = doc["name"].toString();
            payload.contest = doc["group"].toString();
            payload.url = doc["url"].toString();
            payload.interactive = doc["interactive"].toBool();
            payload.memoryLimit = doc["memoryLimit"].toDouble();
            payload.timeLimit = doc["timeLimit"].toDouble();
            payload.isInputstdin = doc["input"].toObject()["type"].toString() == "stdin";
            payload.isOutputstdout = doc["output"].toObject()["type"].toString() == "stdout";

            QJsonArray testArray = doc["tests"].toArray();
            for (auto tests : testArray)
            {
                auto in = tests.toObject()["input"].toString();
                auto out = tests.toObject()["output"].toString();
                payload.testcases.push_back({in, out});
            }
            emit onRequestArrived(payload);
        }
        else
        {
            Log::MessageLogger::error("Companion", "JSONParser reported errors. \n" + error.errorString().toStdString(),
                                      true);
        }
    }
    else
    {
        Log::MessageLogger::warn("Companion", "An Invalid Payload was delivered on the listening port");
        socket->write("HTTP/1.1  OK\r\n"); // \r needs to be before \n
        socket->write("Content-Type: text/html\r\n");
        socket->write("Connection: close\r\n");
        socket->write("Pragma: no-cache\r\n");
        socket->write("\r\n");
        socket->write("<!DOCTYPE html>\r\n");
        socket->write("<html><body><h1>Rejected!!! Only POST requests with Content-type JSON "
                      "are allowed "
                      "here.</h1></body></html>");
        socket->write(QByteArray());
        socket->disconnectFromHost();
    }
}
void CompanionServer::onTerminateConnection()
{
    socket->deleteLater();
}

} // namespace Network
