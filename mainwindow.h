#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QTimer>
#include <QByteArray>
#include <QLabel>
#include <QQueue>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_connectButton_clicked();
    void serialReadyRead();
    void serialErrorOccurred(QSerialPort::SerialPortError error);
    void on_buttonSetFrequency_clicked();
    void on_buttonSetAttenuation_clicked();
    void on_buttonStartStop_clicked();
    void on_zeroButton_clicked();
    void addMeasurementToQueue();
    void addTempToQueue();
    void performZeroCommand();

private:
    void processCommandQueue();
    void updatePowerDisplay();
    void setInterfaceEnabled(bool enabled);
    void resetStatusLabels();

    Ui::MainWindow *ui;
    QSerialPort *serial;
    QTimer *measurementTimer;
    QTimer *zeroDelayTimer;
    QTimer *tempUpdateTimer;
    QByteArray serialBuffer;

    QQueue<QByteArray> commandQueue;
    QByteArray currentCommand;
    QLabel *statusIdLabel;
    QLabel *statusTempLabel;

    bool isDeviceBusy = false;
    bool isMeasuring = false;

    double attenuationDb = 0.0;
    double lastMeasuredPower = 0.0;
};

#endif // MAINWINDOW_H
