#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QLocale>
#include <QDebug>
#include <cmath>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), serial(new QSerialPort(this)) {
    ui->setupUi(this);

    measurementTimer = new QTimer(this);
    zeroDelayTimer = new QTimer(this);
    zeroDelayTimer->setSingleShot(true);
    tempUpdateTimer = new QTimer(this);

    connect(serial, &QSerialPort::readyRead, this, &MainWindow::serialReadyRead);
    connect(serial, &QSerialPort::errorOccurred, this, &MainWindow::serialErrorOccurred);
    connect(measurementTimer, &QTimer::timeout, this, &MainWindow::addMeasurementToQueue);
    connect(tempUpdateTimer, &QTimer::timeout, this, &MainWindow::addTempToQueue);
    connect(zeroDelayTimer, &QTimer::timeout, this, &MainWindow::performZeroCommand);

    ui->comboPort->addItem("Select Port");
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        ui->comboPort->addItem(info.portName());
    }

    statusIdLabel = new QLabel(this);
    statusTempLabel = new QLabel(this);
    ui->statusbar->addPermanentWidget(statusIdLabel);
    ui->statusbar->addPermanentWidget(statusTempLabel, 1);
    resetStatusLabels();
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::processCommandQueue() {
    if (isDeviceBusy || commandQueue.isEmpty()) {
        return;
    }
    isDeviceBusy = true;
    currentCommand = commandQueue.dequeue();
    serial->write(currentCommand);
    ui->textBrowserLog->append("CMD: " + QString(currentCommand).trimmed());
}

void MainWindow::serialReadyRead() {
    serialBuffer.append(serial->readAll());

    while (serialBuffer.contains('\n')) {
        int newlineIndex = serialBuffer.indexOf('\n');
        QByteArray packet = serialBuffer.left(newlineIndex);
        serialBuffer.remove(0, newlineIndex + 1);

        QString reply = QString::fromLatin1(packet).trimmed();
        reply.remove(QChar('\0'));

        if (reply.isEmpty()) {
            ui->textBrowserLog->append("RSP: [empty message]");
            continue;
        }

        ui->textBrowserLog->append("RSP: " + reply);

        if (currentCommand.isEmpty()) {
            continue;
        }

        if (currentCommand == "IDN?\n") {
            if (reply.toUpper() == "NO TERM") {
                commandQueue.enqueue(currentCommand);
            } else {
                if (reply.startsWith("ANRITSU")) {
                    QStringList parts = reply.split(',');
                    if (parts.length() >= 5) {
                        statusIdLabel->setText(QString("ID: %1 | FW: %2").arg(parts[2], parts[4]));
                        tempUpdateTimer->start(10000);
                        addTempToQueue();
                    }
                }
            }
        } else if (currentCommand == "TEMP?\n") {
            if (reply.toUpper() == "NO TERM") {
                commandQueue.enqueue(currentCommand);
            } else {
                bool ok;
                double temp = reply.toDouble(&ok);
                statusTempLabel->setText(ok ? QString("Temp: %1 °C").arg(temp, 0, 'f', 1) : "Temp: Error");
            }
        } else if (currentCommand == "POW?\n") {
            QLocale c_locale(QLocale::C);
            bool ok;
            double measuredPower = c_locale.toDouble(reply, &ok);
            if (ok) {
                lastMeasuredPower = measuredPower;
                updatePowerDisplay();
            }
        } else if (currentCommand.startsWith("CFFREQ")) {
            if (reply.toUpper() == "OK") {
                ui->textBrowserLog->append("Frequency set successfully.");
                if (isMeasuring) {
                    measurementTimer->start(250);
                }
            } else {
                ui->textBrowserLog->append("Failed to set frequency, retrying...");
                commandQueue.enqueue(currentCommand);
            }
        } else if (currentCommand == "ZERO\n") {
            if (reply.toUpper() == "OK") {
                QMessageBox::information(this, "Success", "Zero calibration completed successfully!");
            }
            setInterfaceEnabled(true);
            if (isMeasuring) {
                measurementTimer->start(250);
            }
        }

        currentCommand.clear();
        isDeviceBusy = false;
        processCommandQueue();
    }
}

void MainWindow::on_connectButton_clicked() {
    if (serial->isOpen()) {
        measurementTimer->stop();
        tempUpdateTimer->stop();
        commandQueue.clear();
        currentCommand.clear();
        isDeviceBusy = false;
        serial->close();
        ui->connectButton->setText("Connect");
        setInterfaceEnabled(true);
        resetStatusLabels();
        return;
    }

    if (ui->comboPort->currentIndex() == 0) {
        QMessageBox::warning(this, "Warning", "Please select a COM port!");
        return;
    }

    serial->setPortName(ui->comboPort->currentText());
    serial->setBaudRate(QSerialPort::Baud9600);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);

    if (serial->open(QIODevice::ReadWrite)) {
        ui->connectButton->setText("Disconnect");
        setInterfaceEnabled(true);
        commandQueue.enqueue("IDN?\n");
        processCommandQueue();
    } else {
        QMessageBox::critical(this, "Error", serial->errorString());
    }
}

void MainWindow::addMeasurementToQueue() {
    if (commandQueue.size() < 5) {
        commandQueue.enqueue("POW?\n");
        processCommandQueue();
    }
}

void MainWindow::addTempToQueue() {
    commandQueue.enqueue("TEMP?\n");
    processCommandQueue();
}

void MainWindow::on_buttonSetFrequency_clicked() {
    if (!serial->isOpen()) {
        QMessageBox::warning(this, "Warning", "Port not open!");
        return;
    }
    bool ok;
    double freqMhz = ui->lineEditFrequency->text().toDouble(&ok);
    if (!ok) {
        QMessageBox::warning(this, "Warning", "Invalid frequency value!");
        return;
    }

    measurementTimer->stop();
    ui->textBrowserLog->append("Pausing measurements to set frequency...");

    QByteArray cmd = QString("CFFREQ %1\n").arg(freqMhz / 1000.0).toUtf8();

    QTimer::singleShot(1000, this, [this, cmd]() {
        commandQueue.enqueue(cmd);
        processCommandQueue();
    });
}

void MainWindow::on_buttonStartStop_clicked() {
    if (!serial->isOpen()) {
        QMessageBox::warning(this, "Warning", "Port not open!");
        return;
    }
    isMeasuring = !isMeasuring;
    if (isMeasuring) {
        measurementTimer->start(250);
        ui->buttonStartStop->setText("Stop");
    } else {
        measurementTimer->stop();
        ui->buttonStartStop->setText("Start");
    }
}

void MainWindow::on_buttonSetAttenuation_clicked() {
    bool ok;
    double newAttenuation = ui->lineEditAttenuation->text().toDouble(&ok);
    if (ok) {
        attenuationDb = newAttenuation;
        updatePowerDisplay();
    } else {
        QMessageBox::warning(this, "Warning", "Invalid attenuation value!");
        ui->lineEditAttenuation->setText(QString::number(attenuationDb));
    }
}

void MainWindow::on_zeroButton_clicked() {
    if (!serial->isOpen()) {
        QMessageBox::warning(this, "Warning", "Port not open!");
        return;
    }

    bool wasMeasuring = isMeasuring;
    if (isMeasuring) {
        measurementTimer->stop();
        isMeasuring = false;
        ui->buttonStartStop->setText("Start");
    }

    setInterfaceEnabled(false);
    serialBuffer.clear();
    commandQueue.clear();

    zeroDelayTimer->setSingleShot(true);
    connect(zeroDelayTimer, &QTimer::timeout, this, [this, wasMeasuring]() {
        isMeasuring = wasMeasuring;
        performZeroCommand();
    });
    zeroDelayTimer->start(1000);
}

void MainWindow::performZeroCommand() {
    commandQueue.enqueue("ZERO\n");
    processCommandQueue();
}

void MainWindow::serialErrorOccurred(QSerialPort::SerialPortError error) {
    if (error != QSerialPort::NoError) {
        QMessageBox::critical(this, "Serial Error", serial->errorString());
        currentCommand.clear();
        isDeviceBusy = false;
        processCommandQueue();
    }
}

void MainWindow::resetStatusLabels() {
    statusIdLabel->setText("ID: -- | FW: --");
    statusTempLabel->setText("Temp: -- °C");
    lastMeasuredPower = 0.0;
    updatePowerDisplay();
}

void MainWindow::setInterfaceEnabled(bool enabled) {
    ui->comboPort->setEnabled(enabled);
    ui->connectButton->setEnabled(enabled);
    ui->zeroButton->setEnabled(enabled && serial->isOpen());
    ui->lineEditFrequency->setEnabled(enabled);
    ui->buttonSetFrequency->setEnabled(enabled);
    ui->lineEditAttenuation->setEnabled(enabled);
    ui->buttonSetAttenuation->setEnabled(enabled);
    ui->buttonStartStop->setEnabled(enabled);
}

void MainWindow::updatePowerDisplay() {
    if (lastMeasuredPower == 0.0 && !isMeasuring) {
        ui->labelPowerValue->setText("- dBm");
        ui->labelWattValue->setText("- W");
        return;
    }
    double finalPower = lastMeasuredPower + attenuationDb;
    QString displayTextDbm = QString::number(finalPower, 'f', 2) + " dBm";
    ui->labelPowerValue->setText(displayTextDbm);

    double powerWatts = pow(10.0, finalPower / 10.0) / 1000.0;
    QString displayTextWatts;
    if (powerWatts >= 1000.0) {
        displayTextWatts = QString::number(powerWatts / 1000.0, 'f', 2) + " kW";
    } else if (powerWatts >= 1.0) {
        displayTextWatts = QString::number(powerWatts, 'f', 2) + " W";
    } else if (powerWatts >= 1e-3) {
        displayTextWatts = QString::number(powerWatts * 1000.0, 'f', 3) + " mW";
    } else if (powerWatts >= 1e-6) {
        displayTextWatts = QString::number(powerWatts * 1e6, 'f', 3) + " µW";
    } else if (powerWatts >= 1e-9) {
        displayTextWatts = QString::number(powerWatts * 1e9, 'f', 3) + " nW";
    } else if (powerWatts >= 1e-12) {
        displayTextWatts = QString::number(powerWatts * 1e12, 'f', 3) + " pW";
    } else if (powerWatts >= 1e-15) {
        displayTextWatts = QString::number(powerWatts * 1e15, 'f', 3) + " fW";
    } else {
        displayTextWatts = QString::number(powerWatts, 'e', 3) + " W";
    }
    ui->labelWattValue->setText(displayTextWatts);
}
