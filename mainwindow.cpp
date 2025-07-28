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
    connect(measurementTimer, &QTimer::timeout, this, &MainWindow::performMeasurement);
    connect(zeroDelayTimer, &QTimer::timeout, this, &MainWindow::performZeroCommand);
    connect(tempUpdateTimer, &QTimer::timeout, this, &MainWindow::requestTemperature);

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

void MainWindow::resetStatusLabels() {
    statusIdLabel->setText("ID: -- | FW: --");
    statusTempLabel->setText("Temp: -- °C");
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
    QString displayTextDbm = QString::number(finalPower, 'f', 4) + " dBm";
    ui->labelPowerValue->setText(displayTextDbm);

    double powerWatts = pow(10.0, finalPower / 10.0) / 1000.0;
    QString displayTextWatts;
    if (powerWatts >= 1000.0) {
        displayTextWatts = QString::number(powerWatts / 1000.0, 'f', 3) + " kW";
    } else if (powerWatts >= 1.0) {
        displayTextWatts = QString::number(powerWatts, 'f', 3) + " W";
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

        if (awaitingIdnResponse) {
            if (reply.toUpper() == "NO TERM") {
                ui->textBrowserLog->append("CMD: Retrying IDN?");
                serial->write("IDN?\n");
                continue;
            }
            awaitingIdnResponse = false;
            if (reply.startsWith("ANRITSU")) {
                QStringList parts = reply.split(',');
                if (parts.length() >= 5) {
                    QString deviceId = parts[2];
                    QString firmwareVersion = parts[4];
                    statusIdLabel->setText(QString("ID: %1 | FW: %2").arg(deviceId, firmwareVersion));
                    tempUpdateTimer->start(10000);
                    requestTemperature();
                }
            }
            continue;
        }

        if (awaitingTempResponse) {
            if (reply.toUpper() == "NO TERM") {
                ui->textBrowserLog->append("CMD: Retrying TEMP?");
                serial->write("TEMP?\n");
                continue;
            }
            awaitingTempResponse = false;
            bool ok;
            double temp = reply.toDouble(&ok);
            if (ok) {
                statusTempLabel->setText(QString("Temp: %1 °C").arg(temp, 0, 'f', 1));
            } else {
                statusTempLabel->setText("Temp: Error");
            }
            if (isMeasuring) {
                measurementTimer->start(250);
            }
            continue;
        }

        if (isZeroing) {
            if (reply.toUpper() == "OK") {
                isZeroing = false;
                setInterfaceEnabled(true);
                if (wasMeasuring) {
                    isMeasuring = true;
                    measurementTimer->start(250);
                    ui->buttonStartStop->setText("Stop");
                }
                QMessageBox::information(this, "Success", "Zero calibration completed successfully!");
            }
            continue;
        }

        QLocale c_locale(QLocale::C);
        bool ok;
        double measuredPower = c_locale.toDouble(reply, &ok);

        if (ok) {
            lastMeasuredPower = measuredPower;
            updatePowerDisplay();
        }
    }
}

void MainWindow::on_connectButton_clicked() {
    if (serial->isOpen()) {
        measurementTimer->stop();
        zeroDelayTimer->stop();
        tempUpdateTimer->stop();
        serial->close();
        ui->connectButton->setText("Connect");
        ui->buttonStartStop->setText("Start");
        isMeasuring = false;
        isZeroing = false;
        lastMeasuredPower = 0.0;
        updatePowerDisplay();
        serialBuffer.clear();
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
        awaitingIdnResponse = true;
        serial->write("IDN?\n");
        ui->textBrowserLog->append("CMD: IDN?");
    } else {
        QMessageBox::critical(this, "Error", serial->errorString());
    }
}

void MainWindow::requestTemperature() {
    if (!serial->isOpen()) {
        return;
    }
    if (isMeasuring) {
        measurementTimer->stop();
    }
    awaitingTempResponse = true;
    serial->write("TEMP?\n");
    ui->textBrowserLog->append("CMD: TEMP?");
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

    double freqGhz = freqMhz / 1000.0;
    QString cmd = QString("CFFREQ %1\n").arg(freqGhz);
    serial->write(cmd.toUtf8());
    ui->textBrowserLog->append("CMD: " + cmd.trimmed());
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

void MainWindow::performMeasurement() {
    if (serial->isOpen()) {
        serial->write("POW?\n");
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

    wasMeasuring = isMeasuring;
    if (isMeasuring) {
        measurementTimer->stop();
        isMeasuring = false;
        ui->buttonStartStop->setText("Start");
        ui->textBrowserLog->append("Measurement stopped for zero calibration");
    }

    setInterfaceEnabled(false);
    ui->textBrowserLog->append("Preparing for zero calibration...");
    serialBuffer.clear();
    zeroDelayTimer->start(1000);
}

void MainWindow::performZeroCommand() {
    isZeroing = true;
    serial->write("ZERO\n");
    ui->textBrowserLog->append("CMD: ZERO");
    ui->textBrowserLog->append("Waiting for device response...");
}

void MainWindow::serialErrorOccurred(QSerialPort::SerialPortError error) {
    if (error != QSerialPort::NoError) {
        QMessageBox::critical(this, "Serial Error", serial->errorString());
    }
}
