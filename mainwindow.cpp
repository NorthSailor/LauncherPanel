#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <cmath>
#include <QStateMachine>
#include <QTimer>
#include <QSerialPort>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    machine = new QStateMachine(this);

    sStandby = new QState();
    sArmed = new QState();
    sCountdown = new QState();
    sHalted = new QState();
    sLaunched = new QState();

    sStandby->addTransition(ui->armBtn, SIGNAL(clicked()), sArmed);
    sArmed->addTransition(ui->abortBtn, SIGNAL(clicked()), sStandby);
    sArmed->addTransition(ui->startCdnBtn, SIGNAL(clicked()), sCountdown);

    sCountdown->addTransition(ui->haltBtn, SIGNAL(clicked()), sHalted);
    sCountdown->addTransition(ui->abortBtn, SIGNAL(clicked()), sStandby);
    sCountdown->addTransition(this, SIGNAL(countdownFinished()), sLaunched);

    sHalted->addTransition(ui->resumeBtn, SIGNAL(clicked()), sCountdown);
    sHalted->addTransition(ui->abortBtn, SIGNAL(clicked()), sStandby);

    sLaunched->addTransition(ui->abortBtn, SIGNAL(clicked()), sStandby);

    connect(sCountdown, SIGNAL(entered()), SLOT(countdownStartedOrResumed()));
    connect(sHalted, SIGNAL(entered()), SLOT(countdownHalted()));
    connect(sStandby, SIGNAL(entered()), SLOT(systemReset()));

    machine->addState(sStandby);
    machine->addState(sArmed);
    machine->addState(sCountdown);
    machine->addState(sHalted);
    machine->addState(sLaunched);

    machine->setInitialState(sStandby);
    machine->start();

    sStandby->assignProperty(ui->statusLabel, "text", "<strong>Standby</strong>");
    sStandby->assignProperty(ui->countdownLbl, "visible", false);
    sStandby->assignProperty(ui->armBtn, "visible", true);
    sStandby->assignProperty(ui->startCdnBtn, "visible", false);
    sStandby->assignProperty(ui->abortBtn, "visible", false);
    sStandby->assignProperty(ui->haltBtn, "visible", false);
    sStandby->assignProperty(ui->resumeBtn, "visible", false);
    sStandby->assignProperty(ui->cfgBox, "visible", true);

    sArmed->assignProperty(ui->statusLabel, "text", "<strong>Armed</strong>");
    sArmed->assignProperty(ui->countdownLbl, "visible", true);
    sArmed->assignProperty(ui->armBtn, "visible", false);
    sArmed->assignProperty(ui->startCdnBtn, "visible", true);
    sArmed->assignProperty(ui->abortBtn, "visible", true);
    sArmed->assignProperty(ui->haltBtn, "visible", false);
    sArmed->assignProperty(ui->resumeBtn, "visible", false);
    sArmed->assignProperty(ui->cfgBox, "visible", false);

    sCountdown->assignProperty(ui->statusLabel, "text", "<strong>Launching</strong>");
    sCountdown->assignProperty(ui->countdownLbl, "visible", true);
    sCountdown->assignProperty(ui->armBtn, "visible", false);
    sCountdown->assignProperty(ui->startCdnBtn, "visible", false);
    sCountdown->assignProperty(ui->abortBtn, "visible", true);
    sCountdown->assignProperty(ui->haltBtn, "visible", true);
    sCountdown->assignProperty(ui->resumeBtn, "visible", false);
    sCountdown->assignProperty(ui->cfgBox, "visible", false);

    sHalted->assignProperty(ui->statusLabel, "text", "<strong>Halted</strong>");
    sHalted->assignProperty(ui->countdownLbl, "visible", true);
    sHalted->assignProperty(ui->armBtn, "visible", false);
    sHalted->assignProperty(ui->startCdnBtn, "visible", false);
    sHalted->assignProperty(ui->abortBtn, "visible", true);
    sHalted->assignProperty(ui->haltBtn, "visible", false);
    sHalted->assignProperty(ui->resumeBtn, "visible", true);
    sHalted->assignProperty(ui->cfgBox, "visible", false);

    sLaunched->assignProperty(ui->statusLabel, "text", "<strong>Launched</strong>");
    sLaunched->assignProperty(ui->countdownLbl, "visible", true);
    sLaunched->assignProperty(ui->armBtn, "visible", false);
    sLaunched->assignProperty(ui->startCdnBtn, "visible", false);
    sLaunched->assignProperty(ui->abortBtn, "visible", true);
    sLaunched->assignProperty(ui->haltBtn, "visible", false);
    sLaunched->assignProperty(ui->resumeBtn, "visible", false);
    sLaunched->assignProperty(ui->cfgBox, "visible", false);

    timer = new QTimer(this);
    timer->setInterval(100);
    timer->setSingleShot(false);
    connect(timer, SIGNAL(timeout()), SLOT(updateCountdown()));

    on_cfgApplyBtn_clicked();

    arduinoPort = new QSerialPort("ttyACM0");
    arduinoPort->setBaudRate(QSerialPort::Baud9600);
    arduinoPort->setDataBits(QSerialPort::Data8);
    arduinoPort->setParity(QSerialPort::NoParity);
    arduinoPort->setStopBits(QSerialPort::OneStop);
    arduinoPort->setFlowControl(QSerialPort::NoFlowControl);
    if (!arduinoPort->open(QIODevice::WriteOnly)) {
        qFatal("Failed to connect to the igniter board!");
        exit(1);
    }

    connect(this, SIGNAL(countdownFinished()), SLOT(sendFiringSignal()));
}

MainWindow::~MainWindow()
{
    arduinoPort->close();
    delete ui;
}

void MainWindow::countdownHalted()
{
    timer->stop();
}

void MainWindow::countdownStartedOrResumed()
{
    timer->start();
}

void MainWindow::formatCountdownLabel()
{
    int minutes = abs(deciSecondsRemaining) / 600;
    int seconds = abs(deciSecondsRemaining % 600) / 10;
    int deciSeconds = abs(deciSecondsRemaining) % 10;

    QString text;
    text.sprintf("T%c%02d:%02d.%d", deciSecondsRemaining > 0 ? '-' : '+',
                 minutes,
                 seconds,
                 deciSeconds);
    ui->countdownLbl->setText(text);
}

void MainWindow::updateCountdown()
{
    deciSecondsRemaining--;
    // Format the countdown label right.

    formatCountdownLabel();

    if (deciSecondsRemaining == 0)
        emit countdownFinished();
}

void MainWindow::systemReset()
{
    on_cfgApplyBtn_clicked();
    formatCountdownLabel();
    timer->stop();
}

void MainWindow::sendFiringSignal()
{
    char buffer[3];
    buffer[0] = 'N';
    buffer[1] = (char)(pulseTenths);
    buffer[2] = 'L';
    arduinoPort->write(buffer, 3);
}

void MainWindow::on_cfgApplyBtn_clicked()
{
    deciSecondsRemaining = ui->cfgDurationSpinbox->value() * 10;
    pulseTenths = ui->cfgPulseSpinbox->value() / 100.0f;
    formatCountdownLabel();
}
