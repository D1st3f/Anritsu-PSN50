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
    zeroDelayTimer->setSingleShot(true); // Одноразовий таймер

    connect(serial, &QSerialPort::readyRead, this, &MainWindow::serialReadyRead);
    connect(serial, &QSerialPort::errorOccurred, this, &MainWindow::serialErrorOccurred);
    connect(measurementTimer, &QTimer::timeout, this, &MainWindow::performMeasurement);
    connect(zeroDelayTimer, &QTimer::timeout, this, &MainWindow::performZeroCommand);

    ui->comboPort->addItem("Select Port");
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        ui->comboPort->addItem(info.portName());
    }
}

MainWindow::~MainWindow() {
    delete ui;
}

// Функція для увімкнення/вимкнення інтерфейсу
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

// Функція для оновлення дисплея. Робить обчислення і встановлює текст.
void MainWindow::updatePowerDisplay() {
    qDebug() << "updatePowerDisplay called. lastMeasuredPower:" << lastMeasuredPower << "isMeasuring:" << isMeasuring;

    if (lastMeasuredPower == 0.0 && !isMeasuring) {
        ui->labelPowerValue->setText("- dBm");
        ui->labelWattValue->setText("- W");
        qDebug() << "Display set to '- dBm' and '- W'";
        return;
    }

    double finalPower = lastMeasuredPower + attenuationDb;

    // Відображаємо потужність в dBm
    QString displayTextDbm = QString::number(finalPower, 'f', 4) + " dBm";
    ui->labelPowerValue->setText(displayTextDbm);

    // Обчислюємо і відображаємо потужність у ватах
    // Правильна формула: P(W) = 10^(P(dBm)/10) / 1000
    double powerWatts = pow(10.0, finalPower / 10.0) / 1000.0;

    QString displayTextWatts;

    // Логіка для вибору одиниць вимірювання
    if (powerWatts >= 1000.0) {
        // Кіловати
        displayTextWatts = QString::number(powerWatts / 1000.0, 'f', 3) + " kW";
    } else if (powerWatts >= 1.0) {
        // Вати
        displayTextWatts = QString::number(powerWatts, 'f', 3) + " W";
    } else if (powerWatts >= 1e-3) {
        // Міливати
        displayTextWatts = QString::number(powerWatts * 1000.0, 'f', 3) + " mW";
    } else if (powerWatts >= 1e-6) {
        // Мікровати
        displayTextWatts = QString::number(powerWatts * 1e6, 'f', 3) + " µW";
    } else if (powerWatts >= 1e-9) {
        // Нановати
        displayTextWatts = QString::number(powerWatts * 1e9, 'f', 3) + " nW";
    } else if (powerWatts >= 1e-12) {
        // Піковати
        displayTextWatts = QString::number(powerWatts * 1e12, 'f', 3) + " pW";
    } else if (powerWatts >= 1e-15) {
        // Фемтовати
        displayTextWatts = QString::number(powerWatts * 1e15, 'f', 3) + " fW";
    } else {
        // Дуже малі значення - наукова нотація
        displayTextWatts = QString::number(powerWatts, 'e', 3) + " W";
    }

    ui->labelWattValue->setText(displayTextWatts);

    qDebug() << "Display set to:" << displayTextDbm << "and" << displayTextWatts;
}

void MainWindow::on_buttonSetAttenuation_clicked() {
    bool ok;
    double newAttenuation = ui->lineEditAttenuation->text().toDouble(&ok);
    if (ok) {
        attenuationDb = newAttenuation;
        // Тепер просто викликаємо оновлення дисплея, без спливаючого вікна
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

    // Запам'ятовуємо, чи було включене вимірювання
    wasMeasuring = isMeasuring;

    // Зупиняємо вимірювання, якщо воно було активне
    if (isMeasuring) {
        measurementTimer->stop();
        isMeasuring = false;
        ui->buttonStartStop->setText("Start");
        ui->textBrowserLog->append("Measurement stopped for zero calibration");
        qDebug() << "Measurement stopped, waiting 1 second before sending ZERO command";
    }

    // Блокуємо інтерфейс
    setInterfaceEnabled(false);
    ui->textBrowserLog->append("Preparing for zero calibration...");

    // Очищуємо буфер від можливих залишкових даних
    serialBuffer.clear();

    // Запускаємо таймер затримки на 1 секунду
    zeroDelayTimer->start(1000);
}

// Новий слот для виконання команди ZERO після затримки
void MainWindow::performZeroCommand() {
    // Встановлюємо режим обнулення
    isZeroing = true;

    // Відправляємо команду ZERO
    serial->write("ZERO\n");
    ui->textBrowserLog->append("CMD: ZERO");
    ui->textBrowserLog->append("Waiting for device response...");

    qDebug() << "Zero command sent after delay, waiting for OK response";
}

void MainWindow::serialReadyRead() {
    serialBuffer.append(serial->readAll());

    while (serialBuffer.contains('\n')) {
        int newlineIndex = serialBuffer.indexOf('\n');
        QByteArray packet = serialBuffer.left(newlineIndex);
        serialBuffer.remove(0, newlineIndex + 1);

        QString reply = QString::fromLatin1(packet).trimmed();

        // Видаляємо всі нульові байти з відповіді
        reply.remove(QChar('\0'));

        // Показуємо в лозі всі отримані повідомлення, навіть порожні
        if (reply.isEmpty()) {
            ui->textBrowserLog->append("RSP: [empty message]");
            continue;
        }

        ui->textBrowserLog->append("RSP: " + reply);

        // Перевіряємо, чи це відповідь на команду ZERO
        if (isZeroing) {
            if (reply.toUpper() == "OK") {
                // Успішне обнулення
                isZeroing = false;
                setInterfaceEnabled(true);

                // Відновлюємо вимірювання, якщо воно було активне
                if (wasMeasuring) {
                    isMeasuring = true;
                    measurementTimer->start(250);
                    ui->buttonStartStop->setText("Stop");
                }

                QMessageBox::information(this, "Success", "Zero calibration completed successfully!");
                qDebug() << "Zero calibration completed successfully";
                continue;
            } else {
                qDebug() << "Received response during zeroing:" << reply;
                // Продовжуємо чекати на OK
                continue;
            }
        }

        // Звичайна обробка даних (тільки якщо не в режимі обнулення)
        if (!isZeroing) {
            // Спробуємо парсити як число
            QLocale c_locale(QLocale::C);
            bool ok;
            double measuredPower = c_locale.toDouble(reply, &ok);

            if (ok) {
                // Зберігаємо останній вимір тільки якщо це валідне число
                double oldValue = lastMeasuredPower;
                lastMeasuredPower = measuredPower;
                qDebug() << "Parsed power value:" << measuredPower << "Old value was:" << oldValue;
                qDebug() << "Current attenuation:" << attenuationDb;
                qDebug() << "Final displayed value should be:" << (measuredPower + attenuationDb);
            } else {
                qDebug() << "Failed to parse as number:" << reply;
            }

            // Завжди оновлюємо дисплей після отримання будь-якого повідомлення
            updatePowerDisplay();
            qDebug() << "Display updated. Current lastMeasuredPower:" << lastMeasuredPower;
        }
    }
}

void MainWindow::on_connectButton_clicked() {
    if (serial->isOpen()) {
        measurementTimer->stop();
        zeroDelayTimer->stop(); // Зупиняємо таймер затримки
        serial->close();
        ui->connectButton->setText("Connect");
        ui->buttonStartStop->setText("Start");
        isMeasuring = false;
        isZeroing = false;
        lastMeasuredPower = 0.0;
        updatePowerDisplay(); // Скидаємо обидва дисплеї
        serialBuffer.clear();
        setInterfaceEnabled(true);
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
    } else {
        QMessageBox::critical(this, "Error", serial->errorString());
    }
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

void MainWindow::serialErrorOccurred(QSerialPort::SerialPortError error) {
    if (error != QSerialPort::NoError) {
        QMessageBox::critical(this, "Serial Error", serial->errorString());
    }
}
