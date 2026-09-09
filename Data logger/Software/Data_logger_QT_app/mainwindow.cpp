#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDateTime>
#include <QDebug>
#include <QDir>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setWindowTitle("Data logger");

    startTime = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    dhtCount = adxlCount = 0;
    tempMax = xMax = yMax = zMax = humMax = -100.0;
    tempMin = xMin = yMin = zMin = humMin = 100.0;
    tempSum = humSum = xSum = ySum = zSum = 0.0;

    viewingLiveEnv = true;
    viewingLiveKin = true;

    setupGraphs();

    ui->cmbEnvHistory->addItem("Live Data", -1);
    ui->cmbKinHistory->addItem("Live Data", -1);

    isReceivingFile = false;
    downloadFile = new QFile(this);

    esp32_socket = new QTcpSocket(this);
    reconnectTimer = new QTimer(this);

    connect(esp32_socket, &QTcpSocket::readyRead, this, &MainWindow::readESP32Data);

    connect(esp32_socket, &QTcpSocket::connected, this, [=](){
        ui->btnConnect->setText("Connected");
        ui->btnConnect->setStyleSheet("background-color: green; color: white;");
        reconnectTimer->stop();

        startTime = QDateTime::currentMSecsSinceEpoch() / 1000.0;

        QTimer::singleShot(500, this, [=](){
            if(esp32_socket->state() == QAbstractSocket::ConnectedState) {
                esp32_socket->write("GET_LIST\n");
                esp32_socket->flush();
            }
        });
    });

    connect(esp32_socket, &QTcpSocket::disconnected, this, [=](){
        ui->btnConnect->setText("Disconnected (Hunting...)");
        ui->btnConnect->setStyleSheet("background-color: red; color: white;");
        reconnectTimer->start(3000);
    });

    connect(reconnectTimer, &QTimer::timeout, this, &MainWindow::tryReconnect);
    tryReconnect();
}

MainWindow::~MainWindow()
{
    if(esp32_socket->isOpen()) esp32_socket->close();
    delete ui;
}

void MainWindow::on_btnConnect_clicked()
{
    esp32_socket->abort();
    ui->btnConnect->setText("Connecting...");
    ui->btnConnect->setStyleSheet("");
    esp32_socket->connectToHost(ui->txtIpAddress->text(), 8080);
}

void MainWindow::on_btnRefreshList_clicked()
{
    if (esp32_socket->state() == QAbstractSocket::ConnectedState) {
        esp32_socket->write("GET_LIST\n");
        esp32_socket->flush();
        qDebug() << "Requested updated history list.";
    }
}

void MainWindow::tryReconnect()
{
    if (esp32_socket->state() != QAbstractSocket::ConnectedState) {
        esp32_socket->connectToHost(ui->txtIpAddress->text(), 8080);
    }
}

void MainWindow::setupGraphs()
{
    ui->dhtPlot->addGraph(); ui->dhtPlot->graph(0)->setPen(QPen(Qt::red));
    ui->dhtPlot->addGraph(); ui->dhtPlot->graph(1)->setPen(QPen(Qt::blue));
    ui->adxlPlot->addGraph(); ui->adxlPlot->graph(0)->setPen(QPen(Qt::red));
    ui->adxlPlot->addGraph(); ui->adxlPlot->graph(1)->setPen(QPen(Qt::green));
    ui->adxlPlot->addGraph(); ui->adxlPlot->graph(2)->setPen(QPen(Qt::blue));

    ui->dhtPlot_3->addGraph(); ui->dhtPlot_3->graph(0)->setPen(QPen(Qt::red));
    ui->dhtPlot_3->addGraph(); ui->dhtPlot_3->graph(1)->setPen(QPen(Qt::blue));
    ui->adxlPlot_2->addGraph(); ui->adxlPlot_2->graph(0)->setPen(QPen(Qt::red));
    ui->adxlPlot_2->addGraph(); ui->adxlPlot_2->graph(1)->setPen(QPen(Qt::green));
    ui->adxlPlot_2->addGraph(); ui->adxlPlot_2->graph(2)->setPen(QPen(Qt::blue));
}

void MainWindow::readESP32Data()
{
    static QByteArray buffer;
    buffer.append(esp32_socket->readAll());

    while (buffer.contains('\n')) {
        int lineEnd = buffer.indexOf('\n');
        QString line = QString::fromUtf8(buffer.left(lineEnd)).trimmed();
        buffer.remove(0, lineEnd + 1);

        if (line.isEmpty()) continue;

        if (line.contains("LIST,")) {
            ui->cmbEnvHistory->clear();
            ui->cmbKinHistory->clear();

            ui->cmbEnvHistory->addItem("Live Data", -1);
            ui->cmbKinHistory->addItem("Live Data", -1);

            int startIndex = line.indexOf("LIST,");
            QString listData = line.mid(startIndex);
            QStringList parts = listData.split(',');

            for (int i = 1; i < parts.size(); i++) {
                QStringList pair = parts[i].split(':');
                if (pair.size() == 2) {
                    int daysAgo = pair[0].toInt();
                    int fileId = pair[1].toInt();

                    QString label;
                    if (daysAgo == 0) label = "Today (Log " + QString::number(fileId) + ")";
                    else if (daysAgo == 1) label = "1 day ago (Log " + QString::number(fileId) + ")";
                    else label = QString("%1 days ago (Log %2)").arg(daysAgo).arg(fileId);

                    ui->cmbEnvHistory->addItem(label, fileId);
                    ui->cmbKinHistory->addItem(label, fileId);
                }
            }
            continue;
        }

        if (isReceivingFile) {
            if (line == "END_FILE") {
                isReceivingFile = false;
                downloadFile->close();
                parseHistoricalFile(downloadFile->fileName(), currentDownloadTarget);
            } else {
                if (downloadFile->isOpen()) downloadFile->write(line.toUtf8() + "\n");
            }
            continue;
        }

        if (line.startsWith("START_FILE,")) {
            QStringList parts = line.split(',');
            if (parts.size() >= 2) {
                QString targetFilename = QDir::currentPath() + "/" + parts[1].remove("/");
                downloadFile->setFileName(targetFilename);
                if (downloadFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
                    isReceivingFile = true;
                }
            }
            continue;
        }

        double currentTime = (QDateTime::currentMSecsSinceEpoch() / 1000.0) - startTime;

        if (currentTime >= 60.0) {
            startTime = QDateTime::currentMSecsSinceEpoch() / 1000.0;
            currentTime = 0.0;

            ui->dhtPlot->graph(0)->data()->clear();
            ui->dhtPlot->graph(1)->data()->clear();
            ui->adxlPlot->graph(0)->data()->clear();
            ui->adxlPlot->graph(1)->data()->clear();
            ui->adxlPlot->graph(2)->data()->clear();

            if(viewingLiveEnv) {
                ui->dhtPlot_3->graph(0)->data()->clear();
                ui->dhtPlot_3->graph(1)->data()->clear();
            }
            if(viewingLiveKin) {
                ui->adxlPlot_2->graph(0)->data()->clear();
                ui->adxlPlot_2->graph(1)->data()->clear();
                ui->adxlPlot_2->graph(2)->data()->clear();
            }
        }

        QStringList tokens = line.split(',');

        if (tokens.size() >= 3 && tokens[0] == "DHT") {
            double temp = tokens[1].toDouble(); double hum = tokens[2].toDouble();

            ui->dhtPlot->graph(0)->addData(currentTime, temp);
            ui->dhtPlot->graph(1)->addData(currentTime, hum);
            ui->dhtPlot->rescaleAxes();
            ui->dhtPlot->xAxis->setRange(0, 60);
            ui->dhtPlot->replot();

            if (viewingLiveEnv) {
                dhtCount++;
                if (temp > tempMax) tempMax = temp; if (temp < tempMin) tempMin = temp; tempSum += temp;
                if (hum > humMax) humMax = hum; if (hum < humMin) humMin = hum; humSum += hum;

                ui->dhtPlot_3->graph(0)->addData(currentTime, temp);
                ui->dhtPlot_3->graph(1)->addData(currentTime, hum);
                ui->dhtPlot_3->rescaleAxes();
                ui->dhtPlot_3->xAxis->setRange(0, 60);
                ui->dhtPlot_3->replot();

                QString envText = QString("<h3 style='color:red;'>Live Temp (°C)</h3><b>Current:</b> %1 | <b>Max:</b> %2 | <b>Min:</b> %3 | <b>Avg:</b> %4<br><hr>"
                                          "<h3 style='color:blue;'>Live Humidity (%)</h3><b>Current:</b> %5 | <b>Max:</b> %6 | <b>Min:</b> %7 | <b>Avg:</b> %8")
                                      .arg(temp, 0, 'f', 1).arg(tempMax, 0, 'f', 1).arg(tempMin, 0, 'f', 1).arg(tempSum/dhtCount, 0, 'f', 1)
                                      .arg(hum, 0, 'f', 1).arg(humMax, 0, 'f', 1).arg(humMin, 0, 'f', 1).arg(humSum/dhtCount, 0, 'f', 1);
                ui->envTextBox->setHtml(envText);
            }
        }
        else if (tokens.size() >= 4 && tokens[0] == "ADXL") {
            double x = tokens[1].toDouble(); double y = tokens[2].toDouble(); double z = tokens[3].toDouble();

            ui->adxlPlot->graph(0)->addData(currentTime, x);
            ui->adxlPlot->graph(1)->addData(currentTime, y);
            ui->adxlPlot->graph(2)->addData(currentTime, z);
            ui->adxlPlot->rescaleAxes();
            ui->adxlPlot->xAxis->setRange(0, 60);
            ui->adxlPlot->replot();

            if (viewingLiveKin) {
                adxlCount++;
                if (x > xMax) xMax = x; if (x < xMin) xMin = x; xSum += x;
                if (y > yMax) yMax = y; if (y < yMin) yMin = y; ySum += y;
                if (z > zMax) zMax = z; if (z < zMin) zMin = z; zSum += z;

                ui->adxlPlot_2->graph(0)->addData(currentTime, x);
                ui->adxlPlot_2->graph(1)->addData(currentTime, y);
                ui->adxlPlot_2->graph(2)->addData(currentTime, z);
                ui->adxlPlot_2->rescaleAxes();
                ui->adxlPlot_2->xAxis->setRange(0, 60);
                ui->adxlPlot_2->replot();

                QString kinText = QString("<h3 style='color:red;'>Live X-Axis</h3>Max: %1 | Min: %2 | Avg: %3<br><hr>"
                                          "<h3 style='color:green;'>Live Y-Axis</h3>Max: %4 | Min: %5 | Avg: %6<br><hr>"
                                          "<h3 style='color:blue;'>Live Z-Axis</h3>Max: %7 | Min: %8 | Avg: %9")
                                      .arg(xMax, 0, 'f', 2).arg(xMin, 0, 'f', 2).arg(xSum/adxlCount, 0, 'f', 2)
                                      .arg(yMax, 0, 'f', 2).arg(yMin, 0, 'f', 2).arg(ySum/adxlCount, 0, 'f', 2)
                                      .arg(zMax, 0, 'f', 2).arg(zMin, 0, 'f', 2).arg(zSum/adxlCount, 0, 'f', 2);
                ui->kinTextBox->setHtml(kinText);
            }
        }
    }
}

void MainWindow::parseHistoricalFile(QString filePath, QString targetType)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    double simulatedTime = 0.0;

    if(targetType == "ENV") {
        ui->dhtPlot_3->graph(0)->data()->clear();
        ui->dhtPlot_3->graph(1)->data()->clear();

        double histTMax = -100, histTMin = 100, histTSum = 0;
        double histHMax = -1, histHMin = 101, histHSum = 0;
        int histCount = 0;

        while (!file.atEnd()) {
            QString line = QString::fromUtf8(file.readLine()).trimmed();
            QStringList tokens = line.split(',');
            if (tokens.size() >= 3 && tokens[0] == "DHT") {
                double t = tokens[1].toDouble(); double h = tokens[2].toDouble();
                ui->dhtPlot_3->graph(0)->addData(simulatedTime, t);
                ui->dhtPlot_3->graph(1)->addData(simulatedTime, h);

                if(t > histTMax) histTMax = t; if(t < histTMin) histTMin = t; histTSum += t;
                if(h > histHMax) histHMax = h; if(h < histHMin) histHMin = h; histHSum += h;
                histCount++;
                simulatedTime += 2.0;
            }
        }

        ui->dhtPlot_3->rescaleAxes();
        ui->dhtPlot_3->xAxis->setRange(0, 60);
        ui->dhtPlot_3->replot();

        if (histCount > 0) {
            QString histText = QString("<h3 style='color:red;'>Historical Temp (°C)</h3><b>Max:</b> %1 | <b>Min:</b> %2 | <b>Avg:</b> %3<br><hr>"
                                       "<h3 style='color:blue;'>Historical Humidity (%)</h3><b>Max:</b> %4 | <b>Min:</b> %5 | <b>Avg:</b> %6")
                                   .arg(histTMax, 0, 'f', 1).arg(histTMin, 0, 'f', 1).arg(histTSum/histCount, 0, 'f', 1)
                                   .arg(histHMax, 0, 'f', 1).arg(histHMin, 0, 'f', 1).arg(histHSum/histCount, 0, 'f', 1);
            ui->envTextBox->setHtml(histText);
        }
    }
    else if (targetType == "KIN") {
        ui->adxlPlot_2->graph(0)->data()->clear();
        ui->adxlPlot_2->graph(1)->data()->clear();
        ui->adxlPlot_2->graph(2)->data()->clear();

        double hXMax = -100, hXMin = 100, hXSum = 0;
        double hYMax = -100, hYMin = 100, hYSum = 0;
        double hZMax = -100, hZMin = 100, hZSum = 0;
        int histCount = 0;

        while (!file.atEnd()) {
            QString line = QString::fromUtf8(file.readLine()).trimmed();
            QStringList tokens = line.split(',');
            if (tokens.size() >= 4 && tokens[0] == "ADXL") {
                double x = tokens[1].toDouble(); double y = tokens[2].toDouble(); double z = tokens[3].toDouble();
                ui->adxlPlot_2->graph(0)->addData(simulatedTime, x);
                ui->adxlPlot_2->graph(1)->addData(simulatedTime, y);
                ui->adxlPlot_2->graph(2)->addData(simulatedTime, z);

                if(x > hXMax) hXMax = x; if(x < hXMin) hXMin = x; hXSum += x;
                if(y > hYMax) hYMax = y; if(y < hYMin) hYMin = y; hYSum += y;
                if(z > hZMax) hZMax = z; if(z < hZMin) hZMin = z; hZSum += z;
                histCount++;
                simulatedTime += 0.1;
            }
        }

        ui->adxlPlot_2->rescaleAxes();
        ui->adxlPlot_2->xAxis->setRange(0, 60);
        ui->adxlPlot_2->replot();

        if (histCount > 0) {
            QString histText = QString("<h3 style='color:red;'>Historical X-Axis</h3>Max: %1 | Min: %2 | Avg: %3<br><hr>"
                                       "<h3 style='color:green;'>Historical Y-Axis</h3>Max: %4 | Min: %5 | Avg: %6<br><hr>"
                                       "<h3 style='color:blue;'>Historical Z-Axis</h3>Max: %7 | Min: %8 | Avg: %9")
                                   .arg(hXMax, 0, 'f', 2).arg(hXMin, 0, 'f', 2).arg(hXSum/histCount, 0, 'f', 2)
                                   .arg(hYMax, 0, 'f', 2).arg(hYMin, 0, 'f', 2).arg(hYSum/histCount, 0, 'f', 2)
                                   .arg(hZMax, 0, 'f', 2).arg(hZMin, 0, 'f', 2).arg(hZSum/histCount, 0, 'f', 2);
            ui->kinTextBox->setHtml(histText);
        }
    }
}

void MainWindow::on_btnDownloadEnv_clicked()
{
    int fileId = ui->cmbEnvHistory->currentData().toInt();

    if (fileId == -1) {
        viewingLiveEnv = true;
        ui->dhtPlot_3->graph(0)->data()->clear();
        ui->dhtPlot_3->graph(1)->data()->clear();
        ui->envTextBox->setHtml("<b>Resuming Live Data...</b>");
    } else {
        viewingLiveEnv = false;
        if (esp32_socket->state() == QAbstractSocket::ConnectedState) {
            currentDownloadTarget = "ENV";
            ui->envTextBox->setHtml("<b>Downloading History...</b>");
            esp32_socket->write(QString("GET_LOG_%1\n").arg(fileId).toUtf8());
        }
    }
}

void MainWindow::on_btnDownloadKin_clicked()
{
    int fileId = ui->cmbKinHistory->currentData().toInt();

    if (fileId == -1) {
        viewingLiveKin = true;
        ui->adxlPlot_2->graph(0)->data()->clear();
        ui->adxlPlot_2->graph(1)->data()->clear();
        ui->adxlPlot_2->graph(2)->data()->clear();
        ui->kinTextBox->setHtml("<b>Resuming Live Data...</b>");
    } else {
        viewingLiveKin = false;
        if (esp32_socket->state() == QAbstractSocket::ConnectedState) {
            currentDownloadTarget = "KIN";
            ui->kinTextBox->setHtml("<b>Downloading History...</b>");
            esp32_socket->write(QString("GET_LOG_%1\n").arg(fileId).toUtf8());
        }
    }
}