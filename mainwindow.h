#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class QStateMachine;
class QState;
class QTimer;
class QSerialPort;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void formatCountdownLabel();

private:
    Ui::MainWindow *ui;

protected:
    QStateMachine *machine;
    QState *sStandby;
    QState *sArmed;
    QState *sCountdown;
    QState *sHalted;
    QState *sLaunched;

    int deciSecondsRemaining;
    int pulseTenths;
    QTimer *timer;

    QSerialPort *arduinoPort;

protected slots:
    void countdownHalted();
    void countdownStartedOrResumed();

    void updateCountdown();
    void systemReset();

    void sendFiringSignal();

signals:
    void countdownFinished();
private slots:
    void on_cfgApplyBtn_clicked();
};

#endif // MAINWINDOW_H
