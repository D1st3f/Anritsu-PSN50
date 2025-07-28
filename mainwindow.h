#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QTimer>
#include <QByteArray>
#include <QLabel>

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
    void performMeasurement();
    void performZeroCommand();
    void requestTemperature();

private:
    void updatePowerDisplay();
    void setInterfaceEnabled(bool enabled);
    void resetStatusLabels();

    Ui::MainWindow *ui;
    QSerialPort *serial;
    QTimer *measurementTimer;
    QTimer *zeroDelayTimer;
    QTimer *tempUpdateTimer;
    QByteArray serialBuffer;

    QLabel *statusIdLabel;
    QLabel *statusTempLabel;

    bool isMeasuring = false;
    bool isZeroing = false;
    bool wasMeasuring = false;

    bool awaitingIdnResponse = false;
    bool awaitingTempResponse = false;

    double attenuationDb = 0.0;
    double lastMeasuredPower = 0.0;
};

#endif // MAINWINDOW_H
