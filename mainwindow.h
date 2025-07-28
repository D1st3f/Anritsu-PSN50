#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QTimer>
#include <QByteArray>

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
    void performZeroCommand(); // Новий слот для виконання команди ZERO після затримки

private:
    void updatePowerDisplay();
    void setInterfaceEnabled(bool enabled);

    Ui::MainWindow *ui;
    QSerialPort *serial;
    QTimer *measurementTimer;
    QTimer *zeroDelayTimer; // Новий таймер для затримки перед ZERO
    QByteArray serialBuffer;

    bool isMeasuring = false;
    bool isZeroing = false;
    bool wasMeasuring = false; // Для запам'ятовування стану вимірювання під час обнулення
    double attenuationDb = 0.0;
    double lastMeasuredPower = 0.0;
};

#endif // MAINWINDOW_H
