#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWidget>
#include <QLabel>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QComboBox>
#include <QStandardItemModel>
#include <QMediaPlayer>
#include <QAudioDeviceInfo>
#include <QAudioOutput>
#include <QAudioInput>
#include <QFile>
#include <QTimer>
#include <QDir>
#include <QSpinBox>
#include <QCheckBox>
#include <QFileDialog>
#include <QBuffer>
#include "include/qvumeter.h"
#include "include/reverb.h"
#include "include/passwordlineedit.h"
#include "include/portaudio.h"
#include "include/libsndfile/sndfile.hh"


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

#define EFFECT_SIZE 256 * FRAME_SIZE
const QString recDir =  "Records";


#define nbInstruMax 16
#define fileDetailsMax 10

struct file_Details
{
    QString fileName;
    QTime loop_begin;
    QTime loop_end;
    int sort = -1;
};


struct sample_st
{
    float L; // left channel sample
    float R; // right channel sample
};

class myCombo : public QComboBox
{
protected:
    void keyPressEvent(QKeyEvent *e) {
        if(e->key() == Qt::Key_Delete) { setCurrentIndex(-1); }
    }
};

enum connectionType   { client, server };
enum microMode { Listen, Record, Playback, trackOff };
enum fileMode { loaded, notLoaded };
enum mixMode { mixOff, mixOn };


class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    PasswordLineEdit *pwd;
    PaStreamParameters inputParameters[nbInstruMax], outputParameters[nbInstruMax];
    PaStream *Stream[nbInstruMax];
    QCheckBox *reverbOnly[nbInstruMax];
    QPushButton *Mix[nbInstruMax];
    QPushButton *SaveTrack[nbInstruMax];
    QPushButton *FtpUpload[nbInstruMax];
    QPushButton *ModeButton[nbInstruMax];
    QDialog *mixSetup[nbInstruMax];
    bool mixAlreadyShowed[nbInstruMax];
    QSpinBox *MixSend[nbInstruMax][nbInstruMax];
    int mixValue[nbInstruMax][nbInstruMax];
    QCheckBox *mixCheckAllChannels[nbInstruMax], *mixCheckAll[nbInstruMax];
    myCombo *Ecouteur[nbInstruMax];
    myCombo *Micro[nbInstruMax];
    QString EcouteurSaved[nbInstruMax];
    QString MicroSaved[nbInstruMax];
    QString fileLoaded[nbInstruMax];
    QLineEdit *Nom[nbInstruMax];
    QSpinBox *L_In[nbInstruMax];
    QSpinBox *L_Out[nbInstruMax];
    QLabel *frameIndex[nbInstruMax];
    QVUMeter *vuMeter[nbInstruMax];
    QComboBox *ReverbSelect[nbInstruMax];
    QStringList downloadQueue;
    QList <int> downloadQueueID;
    qint64 delayMean[nbInstruMax];
    bool streamOk[nbInstruMax];
    bool vuMeterEnable = true;
    int firstDevice = 0;
    PaDeviceIndex setDevice(int deviceIndex = 0);
    SF_INFO sfInfo;
    SNDFILE *sndFile = nullptr;
    bool sliderMove = true;
    QList <int> deviceID;
    QList <int> deviceAPI;
    QStringList deviceName;
    QList <int> deviceMicID;
    QList <int> deviceMicAPI;
    QStringList deviceMicName;
    QStringList audioDrivers;
    QString deviceType(PaHostApiTypeId type);
    bool turnedON = false;
    QNetworkAccessManager *webDownload = nullptr;
    QNetworkAccessManager *webUpload = nullptr;
    QNetworkReply *htmlReply = nullptr;
    QNetworkReply *ftpReply = nullptr;
    QByteArray htmlFileList;
    QDate getFileDate(QString);
    QFile *downloadFile = nullptr;
    QFile *uploadFile = nullptr;
    void uploadRecWavToFTP(int);
private:
    Ui::MainWindow *ui;
    QString fileName;
    QTimer refresh;
    void newDevice();
    void loadConfig();
    void saveConfig();
    void setFile(QString);
    void playNow();
    int64_t loadWavFile(QString fileName, float *&wavSound, bool &);
    QColor playButtonColor;
    file_Details fileDetails[fileDetailsMax];
    int fileDetailsIndex = -1;
    void setLoopBegin (quint64 v);
    void setLoopEnd (quint64 v);
    void setRecentList();
    QTime loopIdx2Time(quint64 idx);
    static int Callback(const void *input,void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo* paTimeInfo, PaStreamCallbackFlags statusFlags, void *userData);
    void closeEvent(QCloseEvent *event);
    void updateReplay();
    void setStyleSheet(QPushButton *, int style);
    void setStyleSheet(QLineEdit *button, int style);
    QString getWavRecFileName(int);
    QString checkWavFiles(QString);
    QTimer downloadTimer;
    QString searchDevice();
    QSpinBox *lastSpinChanged = nullptr;
    int lastSpinValue = 0;
    QPoint lastMixOpened;
private slots:
    void chooseFile();
    void ONOFF();
    void ON();
    void OFF();
    void play();
    void stop();
    void record();
    void rewind();
    void gen_metronome();
    void searchDevices();
    void movePosition();
    void stopSlider();
    void sliderMoved(int);
    void sampleRateChange();
    void mixToggle();
    void initToggle();
    void logToggle();
    void vuMeterToggle();
    void driverChanged(int);
    void frameSizeChanged(int);
    void nextDownload();
    void mute();
    void saveFiles(int track = -1);
    void updateT();
    void updateLevel();
    void updateReplay(QPushButton *);
    void showMix(QPushButton *);
    void saveThisTrack(QPushButton *);
    void uploadThisTrack(QPushButton *);
    void nomChanged(QLineEdit *);
    void mixChanged(int);
    void mixAsChanged(QSpinBox *);
    void updateReverb(QComboBox *);
    void updateEcouteur(myCombo *);
    void updateMicro(myCombo *);
    void loopRecordChanged(int);
    void loopEnable(int);
    void reverbOnlyChanged(int);
    void recentListSelect(int);
    void beginChanged(QTime);
    void endChanged(QTime);
    void downloadFinished(QNetworkReply*);
    void listFinished(QNetworkReply*);
    void onDownloadProgress(qint64, qint64);
    void onReadyRead();
    void onReadyReadList();
    void onReplyFinished();
    void listReplyFinished();
    void getWebFileList();
    void ftpUploadProgress(qint64, qint64);
    void ftpUploadDone();
    void ftpRequestFinished(QNetworkReply*);
    void ftpUploadError(QNetworkReply::NetworkError);
protected:
    void keyPressEvent(QKeyEvent *event);
};
#endif // MAINWINDOW_H
