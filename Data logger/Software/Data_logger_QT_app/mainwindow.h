#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QFile>
#include <QTimer>
#include "qcustomplot.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void readESP32Data();
    void tryReconnect();
    void on_btnConnect_clicked();
    void on_btnRefreshList_clicked();
    void on_btnDownloadEnv_clicked();
    void on_btnDownloadKin_clicked();

private:
    Ui::MainWindow *ui;
    QTcpSocket *esp32_socket;
    QTimer *reconnectTimer;
    bool viewingLiveEnv, viewingLiveKin, isReceivingFile;
    QFile *downloadFile;
    QString currentDownloadTarget;
    double startTime;
    int dhtCount, adxlCount;
    double tempMax, tempMin, tempSum, humMax, humMin, humSum;
    double xMax, xMin, xSum, yMax, yMin, ySum, zMax, zMin, zSum;

    void setupGraphs();
    void parseHistoricalFile(QString filePath, QString targetType);
};
#endif