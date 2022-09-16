// Compile :
/*

Portaudio install :

sudo apt-get install libasound-dev

Télécharger portaudio

sudo sh configure && make
cp lib/.libs/libportaudio.a /home/remy/Remixer/lib
sudo ldconfig
sudo apt install libportaudio2

libsndfile install Windows with CMake

sudo configure
sudo make
sudo make install

Ubuntu : sudo apt-get install -y libsndfile-dev


Pour faire tourner sous Ubuntu :

sudo apt install libportaudio2
*/



#include <QDateTime>
#include <QInputDialog>
#include <QMessageBox>
#include <QLineEdit>
#include <QAudioDecoder>
#include <QDesktopWidget>
#include <QSettings>
#include "include/AudioIO.hh"
#include "include/reverb.c"
#include <math.h>
#include "mainwindow.h"
#include "ui_mainwindow.h"

int FRAME_SIZE = 256;

/*struct mix_frame
{
    sf_sample_st frame[32];
};*/

struct mix_frame
{
    sf_sample_st *frame= new sf_sample_st[FRAME_SIZE];
};

struct mix_track_list
{
    QVector <mix_frame*> mix_send_list; // List of sample populated by different tracks, read back for cross mix
                                         // When input buffer is available (or recorded wav if read back is selected instead of live sound)
                                         // one pointer mix_send is created, sound is copied to the array with send gain, destination track will have to read it and delete it from the list
                                         // if input buffer arrives before array an been read and taken, new pointer is created and added to the list
                                         // List of sample populated by different tracks, read back for cross mix
};

mix_track_list mix_track_Hor_Vert[nbInstruMax][nbInstruMax];


struct pa_Data
{
     quint64 frames;
     float *wavRecord;
     sf_reverb_state_st *rv;
     sf_sample_st *buffer_in;
     sf_sample_st *buffer_out;
     quint64 position = 0;
     float G_In = 1;
     float G_Play = 1;
     float G_Rec = 1;
     float G_RevLev = 1;
     int RecVol = 0;
     int RevLev = 0;
     int PlayVol = 0;
     int ListenVol = 0;
     float G_Wav = 1;
     float levelLeft = 0;
     float levelRight = 0;
     int mode;
     bool reverb = true;
     bool recorded = false;
     //bool ftpDone = false;
     int myIndex = -1;
     int *mix_send_Vol;
     float *G_mix_send;
     float mixGain[nbInstruMax];
     QVector <mix_frame*> mix_del_list;
     bool runing = false;
     bool fileLoaded = false;
     bool mixEnabled = false;
     bool reverbOnly = false;
};

sf_sample_st *wav_st = nullptr;     // pointer of an array of sample used for recording, will be initialize to sound size
bool muteState = false;
bool Paused = false;
bool theEnd = false;
bool Runing = false;
bool finished = false;
bool Recording = false;
bool loopRecord = false;
bool loop = false;
quint64 loop_begin = -1;
quint64 loop_end = -1;
bool mixEnabled = false;
bool initEnabled = false;
int64_t Frames = 0;


int instru = -1;
pa_Data pa_data[nbInstruMax];
int nbInstru = 0;
int SAMPLE_RATE = 48000;

#define positionBegin loop_begin
#define positionEnd loop_end
#define title "Remyxer 1.08"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle(title);

    pwd = new PasswordLineEdit(ui->centralwidget);
    pwd->setObjectName(QString::fromUtf8("lineEditFTPpwd"));
    //ui->sizePolicy3.setHeightForWidth(pwd->sizePolicy().hasHeightForWidth());
    //lineEditFTPpwd->setSizePolicy(sizePolicy3);
    ui->gridLayout_3->addWidget(pwd, 11, 2, 1, 1);

    ui->log->hide();
    ui->lineEditDownloadSite->hide();
    ui->labelDownloadSite->hide();
    ui->lineEditUploadSite->hide();
    ui->labelUploadSite->hide();
    ui->comboBoxDriver->hide();
    ui->labelFTPLogin->hide();
    ui->labelFTPpwd->hide();
    ui->labelLatency->hide();
    ui->spinBoxLatency->hide();
    ui->labelFrameSize->hide();
    ui->spinBoxFrameSize->hide();
    ui->lineEditFTPLogin->hide();
    ui->buttonSave->setEnabled(false);
    pwd->hide();
    ui->buttonInit->hide();
    setFocus();

    ui->buttonMix->setStyleSheet("QPushButton { background-color:#cccccc }");
    //ui->groupBoxSampleRate->setStyleSheet("QGroupBox { border-radius: 10px; }");

    ui->buttonPlay->setStyleSheet("QPushButton:checked { background-color: lightgrey; }");
    ui->buttonPlay->setCheckable(true);

    ui->buttonRecord->setStyleSheet("QPushButton:checked { background-color: red; }");
    ui->buttonRecord->setCheckable(true);

    ui->buttonInit->setToolTip("Use a specific file to save configuration");

    playButtonColor = palette().color(QPalette::Button);
    connect(ui->buttonLoad, SIGNAL(clicked()), this, SLOT(chooseFile()));
    connect(ui->buttonPlay, SIGNAL(clicked()), this, SLOT(play()));
    connect(ui->buttonSave, SIGNAL(clicked()), this, SLOT(saveFiles()));
    connect(ui->buttonHP, SIGNAL(clicked()), this, SLOT(mute()));
    connect(ui->buttonRecord, SIGNAL(clicked()), this, SLOT(record()));
    connect(ui->buttonONOFF, SIGNAL(clicked()), this, SLOT(ONOFF()));
    connect(ui->buttonRewind, SIGNAL(clicked()), this, SLOT(rewind()));
    connect(ui->buttonMetronome, SIGNAL(clicked()), this, SLOT(gen_metronome()));
    connect(ui->buttonSearchDevice, SIGNAL(clicked()), this, SLOT(searchDevices()));
    connect(ui->buttonWebUpdate, SIGNAL(clicked()), this, SLOT(getWebFileList()));
    connect(ui->wavSlider, SIGNAL(sliderReleased()), this, SLOT(movePosition()));
    connect(ui->wavSlider, SIGNAL(sliderPressed()), this, SLOT(stopSlider()));
    connect(ui->wavSlider, SIGNAL(sliderMoved(int)), this, SLOT(sliderMoved(int)));
    connect(ui->radioButton44, SIGNAL(released()), this, SLOT(sampleRateChange()));
    connect(ui->radioButton48, SIGNAL(released()), this, SLOT(sampleRateChange()));
    connect(ui->buttonMix, SIGNAL(clicked()), this, SLOT(mixToggle()));
    connect(ui->buttonInit, SIGNAL(clicked()), this, SLOT(initToggle()));
    connect(ui->buttonLog, SIGNAL(clicked()), this, SLOT(logToggle()));
    connect(ui->buttonVuMeter, SIGNAL(clicked()), this, SLOT(vuMeterToggle()));
    connect(ui->comboBoxDriver, SIGNAL(currentIndexChanged(int)), this, SLOT(driverChanged(int)));
    connect(ui->spinBoxFrameSize, SIGNAL(valueChanged(int)), this, SLOT(frameSizeChanged(int)));

    downloadTimer.setSingleShot(true);
    connect(&downloadTimer, SIGNAL(timeout()), this, SLOT(nextDownload()));
    ui->timeBegin->setDisplayFormat("mm:ss:z");
    connect(ui->timeBegin, SIGNAL(timeChanged(QTime)), this, SLOT(beginChanged(QTime)));
    ui->timeEnd->setDisplayFormat("mm:ss:z");
    connect(ui->timeEnd, SIGNAL(timeChanged(QTime)), this, SLOT(endChanged(QTime)));
    connect(&refresh, SIGNAL(timeout()), this, SLOT(updateT()));
    ui->buttonRecord->setEnabled(false);
    PaError err;
    err = Pa_Initialize();
    if( err != paNoError ) qWarning() << QString("Error Pa_Initialize %1").arg(err);
    else {
        PaHostApiIndex apiCount = Pa_GetHostApiCount();
        ui->log->append(QString("API Count %1").arg(apiCount));
        for (int api=0; api<apiCount; api++) {
            QString str = Pa_GetHostApiInfo(api)->name;
            ui->comboBoxDriver->addItem(str);
            str.append(" devices :");
            ui->log->append(str);
            int nApiDev = Pa_GetHostApiInfo(api)->deviceCount;
            const PaDeviceInfo *deviceInfo;
            for (int dev=0; dev<nApiDev; dev++) {
                deviceInfo = Pa_GetDeviceInfo(Pa_HostApiDeviceIndexToDeviceIndex(api, dev));
                ui->log->append(deviceInfo->name + QString(" Input : %1 Output : %2 default Sample Rate : %3").arg(deviceInfo->maxInputChannels).arg(deviceInfo->maxOutputChannels).arg(deviceInfo->defaultSampleRate));
#if defined(Q_OS_LINUX)
                if ((deviceInfo->maxOutputChannels > 1 ) && (deviceInfo->maxInputChannels > 0))
                {
                    deviceName.append(deviceType(Pa_GetHostApiInfo(api)->type) + " : " + deviceInfo->name + QString(" %1").arg(deviceInfo->defaultSampleRate));
                    deviceID.append(dev);
                    deviceAPI.append(api);
                }
#endif
#if defined(Q_OS_WIN)
                if (deviceInfo->maxOutputChannels == 2 )
                {
                    deviceName.append(deviceType(Pa_GetHostApiInfo(api)->type) + " : " + deviceInfo->name + QString(" %1").arg(deviceInfo->defaultSampleRate));
                    deviceID.append(dev);
                    deviceAPI.append(api);
                }
                if (deviceInfo->maxInputChannels > 0)
                {
                    deviceMicName.append(deviceType(Pa_GetHostApiInfo(api)->type) + " : " + deviceInfo->name + QString(" %1").arg(deviceInfo->defaultSampleRate));
                    deviceMicID.append(dev);
                    deviceMicAPI.append(api);
                }
#endif
            }
        }
    }
    err = Pa_Terminate();
    if( err != paNoError ) ui->log->append(QString("Error Pa_Terminate %1").arg(err));
    loadConfig();
    QSettings settings("Remyxer.ini", QSettings::IniFormat);
    for (int n=0; n<nbInstru; n++) {
        for (int i=0; i<nbInstru; i++) {
            MixSend[n][i]->setSpecialValueText(Nom[i]->text() +  " : OFF");
            MixSend[n][i]->setPrefix((Nom[i]->text() +  " : "));
            connect(Nom[n], &QLineEdit::textChanged, [=] { nomChanged( Nom[n] );  } );
            bool ok;
            int mix = settings.value(QString("/mix_" + Nom[i]->text() + "/%1").arg(n+1)).toInt(&ok);
            if (ok) MixSend[i][n]->setValue(mix);
            if (mix == -41) pa_data[n].mixGain[i] = 0;
            else pa_data[n].mixGain[i] = double(pow(10.0, double(mix /20.0)));
            connect(MixSend[n][i], SIGNAL(valueChanged(int)), this, SLOT(mixChanged(int)));
            connect(MixSend[n][i], QOverload<int>::of(&QSpinBox::valueChanged), [=] { mixAsChanged( MixSend[n][i] );  } );
        } }
    OFF();
}



MainWindow::~MainWindow()
{
    Pa_Terminate();
    QFile file("Remyxer.ini");
    file.remove("Remyxer.ini");
    saveConfig();
    delete ui;
}



void MainWindow::searchDevices()
{
    while(!searchDevice().isEmpty());
}


QString MainWindow::searchDevice()
{
    if (Runing) return "";
    PaError err;
    err = Pa_Initialize();
    if( err != paNoError ) qWarning() << QString("Error Pa_Initialize %1").arg(err);
    else {
        PaHostApiIndex apiCount = Pa_GetHostApiCount();
        ui->log->append(QString("API Count %1").arg(apiCount));
        for (int api=0; api<apiCount; api++) {
            QString str = Pa_GetHostApiInfo(api)->name;
            str.append(" devices :");
            ui->log->append(str);
            int nApiDev = Pa_GetHostApiInfo(api)->deviceCount;
            QString addMe;
            const PaDeviceInfo *deviceInfo;
            for (int dev=0; dev<nApiDev; dev++) {
                deviceInfo = Pa_GetDeviceInfo(Pa_HostApiDeviceIndexToDeviceIndex(api, dev));
                ui->log->append(deviceInfo->name + QString(" Input : %1 Output : %2 default Sample Rate : %3").arg(deviceInfo->maxInputChannels).arg(deviceInfo->maxOutputChannels).arg(deviceInfo->defaultSampleRate));
#if defined(Q_OS_LINUX)
                if ((deviceInfo->maxOutputChannels > 1 ) && (deviceInfo->maxInputChannels > 0))
                {
                    QString name = deviceType(Pa_GetHostApiInfo(api)->type) + " : " + deviceInfo->name + QString(" %1").arg(deviceInfo->defaultSampleRate);
                    if (!deviceName.contains(name)) {
                    deviceName.append(name);
                    deviceID.append(dev);
                    deviceAPI.append(api);
                    for (int i=0; i<nbInstru; i++) Ecouteur[i]->addItem(name);
                    err = Pa_Terminate();
                    if( err != paNoError ) ui->log->append(QString("Error Pa_Terminate %1").arg(err));
                    return name;
                    }
                }
#endif
#if defined(Q_OS_WIN)
                QString newName;
                if (deviceInfo->maxOutputChannels > 1 )
                {
                    QString name = deviceType(Pa_GetHostApiInfo(api)->type) + " : " + deviceInfo->name + QString(" %1").arg(deviceInfo->defaultSampleRate);
                    if (!deviceName.contains(name)) {
                        deviceName.append(name);
                        deviceID.append(dev);
                        deviceAPI.append(api);
                        for (int i=0; i<nbInstru; i++) Ecouteur[i]->addItem(name);
                        if (Pa_GetHostApiInfo(api)->type == paDirectSound) addMe = name;
                        }
                    }
                if (deviceInfo->maxInputChannels > 0)
                {
                    QString name = deviceType(Pa_GetHostApiInfo(api)->type) + " : " + deviceInfo->name + QString(" %1").arg(deviceInfo->defaultSampleRate);
                    if (!deviceMicName.contains(name)) {
                        deviceMicName.append(name);
                        deviceMicID.append(dev);
                        deviceMicAPI.append(api);
                        for (int i=0; i<nbInstru; i++) Micro[i]->addItem(name);
                        }
                    }
                err = Pa_Terminate();
                if( err != paNoError ) ui->log->append(QString("Error Pa_Terminate %1").arg(err));
                return addMe;
#endif
            }
        }
    }
    err = Pa_Terminate();
    if( err != paNoError ) ui->log->append(QString("Error Pa_Terminate %1").arg(err));
    return "";
}


QString MainWindow::deviceType(PaHostApiTypeId type)
{
    switch (type)
    {
        case paInDevelopment : return "Development";
        case paDirectSound : return "Direct Sound";
        case paMME : return "MME";
        case paASIO : return "ASIO";
        case paSoundManager : return "Sound Manager";
        case paCoreAudio : return "Core Audio";
        case paOSS : return "OSS";
        case paALSA : return "ALSA";
        case paAL : return "AL";
        case paBeOS : return "BeOS";
        case paWDMKS : return "WDKMS";
        case paJACK : return "JACK";
        case paWASAPI : return "WASAPI";
        case paAudioScienceHPI : return "AudioScienceHPI";
        case paAudioIO : return "AudioIO";
    }
    return "";
}


void MainWindow::newDevice()
{
    int index = nbInstru;
    nbInstru ++;
    int x = 0;
    pa_data[index].rv = new sf_reverb_state_st;
    pa_data[index].mode = Listen;
    pa_data[index].myIndex = index;
    ui->gridLayout->setColumnStretch(0, 0);
    ui->gridLayout->setColumnStretch(1, 0);
    ui->gridLayout->setColumnStretch(2, 0);
    ui->gridLayout->setColumnStretch(4, 4);
    ui->gridLayout->setColumnStretch(5, 2);
    ModeButton[index] = new QPushButton(ui->groupBox);
    ModeButton[index]->setIcon(QPixmap(":/images/microvert"));
    ModeButton[index]->setToolTip("Listen");
    ModeButton[index]->setIconSize(QSize(35, 35));
    ModeButton[index]->setFocusPolicy(Qt::NoFocus);
    //ModeButton[index]->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    connect(ModeButton[index], &QPushButton::clicked, [=] { updateReplay( ModeButton[index] );  } );
    Nom[index] = new QLineEdit(ui->groupBox);
    Nom[index]->setText(QString("%1").arg(index+1));
    Nom[index]->setFocusPolicy(Qt::ClickFocus);
    Nom[index]->setStyleSheet("QLineEdit { border-width: 2px; border-radius: 10px; min-width: 10em; } ");
    ui->gridLayout->addWidget(Nom[index], index*2, x, 1, 3);
    Mix[index] = new QPushButton(ui->groupBox);
    connect(Mix[index], &QPushButton::clicked, [=] { showMix( Mix[index] );  } );
    Mix[index]->setIcon(QPixmap(":/images/mixer"));
    Mix[index]->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    Mix[index]->setFocusPolicy(Qt::ClickFocus);
    Mix[index]->setToolTip("Click + Shift to open mixer\nClick with Ctrl key pressed to add last new USB device plugged in");
    ui->gridLayout->addWidget(Mix[index], index*2+1, x++, 1, 1);
    SaveTrack[index] = new QPushButton(ui->groupBox);
    connect(SaveTrack[index], &QPushButton::clicked, [=] { saveThisTrack( SaveTrack[index] );  } );
    ui->gridLayout->addWidget(SaveTrack[index], index*2+1, x++, 1, 1);
    SaveTrack[index]->setIcon(QPixmap(":/images/save"));
    SaveTrack[index]->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    SaveTrack[index]->setFocusPolicy(Qt::ClickFocus);
    SaveTrack[index]->setToolTip("Click to save track to wav file");
    SaveTrack[index]->setEnabled(false);
    FtpUpload[index] = new QPushButton(ui->groupBox);
    connect(FtpUpload[index], &QPushButton::clicked, [=] { uploadThisTrack( FtpUpload[index] );  } );
    ui->gridLayout->addWidget(FtpUpload[index], index*2+1, x++, 1, 1);
    FtpUpload[index]->setIcon(QPixmap(":/images/ftpupload"));
    FtpUpload[index]->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    FtpUpload[index]->setFocusPolicy(Qt::ClickFocus);
    FtpUpload[index]->setToolTip("Click to upload file to the FTP server");
    FtpUpload[index]->setEnabled(false);
    Ecouteur[index] = new myCombo();
    Ecouteur[index]->setFocusPolicy(Qt::ClickFocus);
    //ui->gridLayout->addWidget(Ecouteur[index], index*2+1, x++, 1, 1);
    Micro[index] = new myCombo();
    Micro[index]->setFocusPolicy(Qt::ClickFocus);
#if defined(Q_OS_WIN)
    ui->gridLayout->addWidget(Ecouteur[index], index*2, x, 1, 2);
    ui->gridLayout->addWidget(Micro[index], index*2, x+2, 1, 3);
#else
    ui->gridLayout->addWidget(Ecouteur[index], index*2, x, 1, 5);
#endif
    L_In[index] = new QSpinBox(ui->groupBox);
    L_In[index]->setMinimum(-40);
    L_In[index]->setMaximum(20);
    L_In[index]->setSuffix(" dB");
    L_In[index]->setStyleSheet("QSpinBox { background-color:#a6ff4d }");
    L_In[index]->setPrefix("Mic : ");
    L_In[index]->setSpecialValueText("OFF");
    L_In[index]->setFocusPolicy(Qt::ClickFocus);
    ui->gridLayout->addWidget(L_In[index], index*2+1, x++, 1, 1);
    L_Out[index] = new QSpinBox(ui->groupBox);
    L_Out[index]->setMinimum(-40);
    L_Out[index]->setMaximum(20);
    L_Out[index]->setSuffix(" dB");
    L_Out[index]->setStyleSheet("QSpinBox { background-color:#ffd866 }");
    L_Out[index]->setPrefix("File : ");
    L_Out[index]->setFocusPolicy(Qt::ClickFocus);
    ui->gridLayout->addWidget(L_Out[index], index*2+1, x++, 1, 1);

    Reverb_Level[index] = new QSpinBox(ui->groupBox);
    Reverb_Level[index]->setMinimum(-40);
    Reverb_Level[index]->setMaximum(20);
    Reverb_Level[index]->setSuffix(" dB");
    Reverb_Level[index]->setPrefix("Reverb : ");
    Reverb_Level[index]->setFocusPolicy(Qt::ClickFocus);
    Reverb_Level[index]->setStyleSheet("QSpinBox { background-color: lightgrey }");

    ui->gridLayout->addWidget(Reverb_Level[index], index*2+1, x++, 1, 1);

    reverbOnly[index] = new QCheckBox;
    reverbOnly[index]->setText("Reverb only");
    ui->gridLayout->addWidget(reverbOnly[index], index*2+1, x++, 1, 1);
    connect(reverbOnly[index], SIGNAL(stateChanged(int)), this, SLOT(reverbOnlyChanged(int)));
    frameIndex[index] = new QLabel(ui->groupBox);
    frameIndex[index]->setText("...");
    frameIndex[index]->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
    frameIndex[index]->setFixedSize(60, 20);
    ui->gridLayout->addWidget(frameIndex[index], index*2+1, x++, 1, 1);
    vuMeter[index] = new QVUMeter(ui->groupBox);
    vuMeter[index]->setMaxValue(1.0);
    vuMeter[index]->setMinValue(0.0);
    vuMeter[index]->setFocusPolicy(Qt::NoFocus);
    ui->gridLayout->addWidget(vuMeter[index], index*2, x++, 2, 4);
    vuMeter[index]->setEnabled(false);
    //vuMeter[index]->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Minimum);
    x+=4;
    ui->gridLayout->addWidget(ModeButton[index], index*2, x, 1, 1);
    connect(L_In[index], SIGNAL(valueChanged(int)), this, SLOT(updateLevel()));
    connect(L_Out[index], SIGNAL(valueChanged(int)), this, SLOT(updateLevel()));
    connect(Reverb_Level[index], SIGNAL(valueChanged(int)), this, SLOT(updateReverb()));
    ReverbSelect[index] = new QComboBox(ui->groupBox);
    ReverbSelect[index]->addItem("None");
    ReverbSelect[index]->addItem("Default");
    ReverbSelect[index]->addItem("Small Hall 1");
    ReverbSelect[index]->addItem("Small Hall 2");
    ReverbSelect[index]->addItem("Medium Hall 1");
    ReverbSelect[index]->addItem("Medium Hall 2");
    ReverbSelect[index]->addItem("Large Hall 1");
    ReverbSelect[index]->addItem("Large Hall 2");
    ReverbSelect[index]->addItem("Small Room 1");
    ReverbSelect[index]->addItem("Small Room 2");
    ReverbSelect[index]->addItem("Medium Room 1");
    ReverbSelect[index]->addItem("Medium Room 2");
    ReverbSelect[index]->addItem("Large Room 1");
    ReverbSelect[index]->addItem("Large Room 2");
    ReverbSelect[index]->addItem("Medium ER1");
    ReverbSelect[index]->addItem("Medium ER2");
    ReverbSelect[index]->addItem("Plate High");
    ReverbSelect[index]->addItem("Plate Low");
    ReverbSelect[index]->addItem("Long Reverb 1");
    ReverbSelect[index]->addItem("Long Reverb 2");
    ReverbSelect[index]->setCurrentIndex(8);
    ReverbSelect[index]->setFocusPolicy(Qt::NoFocus);
    ui->gridLayout->addWidget(ReverbSelect[index], index*2+1, x++, 1, 1);
    connect(ReverbSelect[index], &QComboBox::currentTextChanged, [=] { updateReverb( ReverbSelect[index] );  } );
    for(int i=0; i<deviceName.count(); i++ ) Ecouteur[index]->addItem(deviceName.at(i));
    Ecouteur[index]->setCurrentIndex(-1);
    connect(Ecouteur[index], &myCombo::currentTextChanged, [=] { updateEcouteur( Ecouteur[index] );  } );
    for(int i=0; i<deviceMicName.count(); i++ ) Micro[index]->addItem(deviceMicName.at(i));
    Micro[index]->setCurrentIndex(-1);
    connect(Micro[index], &myCombo::currentTextChanged, [=] { updateMicro( Micro[index] );  } );
    connect(ui->checkBoxLoop, SIGNAL(stateChanged(int)), this, SLOT(loopEnable(int)));
    connect(ui->checkBoxLoopRecord, SIGNAL(stateChanged(int)), this, SLOT(loopRecordChanged(int)));
}


PaDeviceIndex MainWindow::setDevice(int deviceIndex){
    PaHostApiIndex alsaInd = Pa_HostApiTypeIdToHostApiIndex(paALSA);
    //PaHostApiIndex DirectSoundInd = Pa_HostApiTypeIdToHostApiIndex(paDirectSound);
    const PaHostApiInfo* apiInf = Pa_GetHostApiInfo(alsaInd);

    if(!apiInf) throw Pa_NoApiException();
    if(-1 == deviceIndex) return apiInf->defaultOutputDevice;

    if(deviceIndex > apiInf->deviceCount ||  deviceIndex < 0){
        throw Pa_DeviceIndexNotFoundException();
    }
    return Pa_HostApiDeviceIndexToDeviceIndex(alsaInd, deviceIndex);
}



void MainWindow::loadConfig()
{
    int x, y, w,h;
    QSettings settings("Remyxer.ini", QSettings::IniFormat);
    w = settings.value( QString::fromUtf8("/geometry/width"), width() ).toInt();
    h = settings.value( QString::fromUtf8("/geometry/height"), height() ).toInt();
    x = settings.value( QString::fromUtf8("/geometry/posx"), this->x() ).toInt();
    y = settings.value( QString::fromUtf8("/geometry/posy"), this->y() ).toInt();
    setGeometry(x, y, w, h);
    bool ok;
    QString n = settings.value(QString("/nbInstru")).toString();
    int nb = n.toInt(&ok);
    if (!ok) nb = 4;
    if (nb > nbInstruMax) nb = nbInstruMax;
    for (int index=0; index<nb; index++) newDevice();
    bool M = settings.value(QString::fromUtf8("/MixEnabled")).toBool();
    if (ok && M) { mixToggle(); }
    QString l = settings.value(QString("/Latency")).toString();
    int L = l.toInt(&ok);
    if (ok) ui->spinBoxLatency->setValue(L);
    QString f = settings.value(QString("/FrameSize")).toString();
    int F = f.toInt(&ok);
    if (ok) {
        ui->spinBoxFrameSize->setValue(F);
        FRAME_SIZE = F; }
    bool SR = settings.value(QString::fromUtf8("/44k")).toBool();
    if (ok && SR) ui->radioButton44->setChecked(true);
    if (ui->radioButton48->isChecked()) SAMPLE_RATE = 48000; else SAMPLE_RATE = 44100;
    QString audioDriver = settings.value( QString::fromUtf8("/audiodriver")).toString();
    if (!audioDriver.isEmpty()) ui->comboBoxDriver->setCurrentText(audioDriver);
    for (int n=0; n<nbInstru; n++) {
        mixSetup[n] = new QDialog();
        mixAlreadyShowed[n] = false;
        mixCheckAll[n] = new QCheckBox();
        mixCheckAll[n]->setText("change All");
        mixCheckAllChannels[n] = new QCheckBox();
        mixCheckAllChannels[n]->setText("change All channels");
        QGridLayout *layout = new QGridLayout(mixSetup[n]);
        layout->addWidget(mixCheckAll[n], 0, 0, 1, 1);
        layout->addWidget(mixCheckAllChannels[n], 0, 2, 1, 1);
        for (int i=0; i<nbInstru; i++) {
            MixSend[n][i] = new QSpinBox();
            mixValue[n][i] = -99;
            MixSend[n][i]->setRange(-41, 20);
            MixSend[n][i]->setSpecialValueText(Nom[i]->text() +  " : OFF");
            MixSend[n][i]->setValue(-41);
            MixSend[n][i]->setPrefix((Nom[i]->text() +  " : "));
            MixSend[n][i]->setSuffix((" dB"));
            layout->addWidget(MixSend[n][i], i/4 + 1, i%4, 1, 1);
        if (n == i) MixSend[n][i]->setEnabled(false); }
        pa_data[n].mix_send_Vol = new int[nbInstru];
        for (int i=0; i<nbInstru; i++) pa_data[n].mix_send_Vol[i] = 0;
        pa_data[n].G_mix_send = new float[nbInstru];
        for (int i=0; i<nbInstru; i++) pa_data[n].G_mix_send[i] = 1;
        Stream[n] = nullptr;
        QString str = settings.value(QString("/nom/%1").arg(n+1)).toString();
        if (!str.isEmpty()) Nom[n]->setText(str);
        mixSetup[n]->setWindowTitle(Nom[n]->text());

        QString device = settings.value(QString("/ecouteur/%1").arg(n+1)).toString();
        EcouteurSaved[n] = device;
        Ecouteur[n]->setCurrentText(device);

        QString deviceIn = settings.value(QString("/micro/%1").arg(n+1)).toString();
        MicroSaved[n] = deviceIn;
        Micro[n]->setCurrentText(deviceIn);

        str = settings.value(QString("/ListenVol/%1").arg(n+1)).toString();
        int v = str.toInt(&ok);
        if (ok) { pa_data[n].ListenVol = v; L_In[n]->setValue(v); }

        str = settings.value(QString("/RecVol/%1").arg(n+1)).toString();
        v = str.toInt(&ok);
        if (ok) pa_data[n].RecVol = v;

        str = settings.value(QString("/RevLev/%1").arg(n+1)).toString();
        v = str.toInt(&ok);
        if (ok) { pa_data[n].RevLev = v; Reverb_Level[n]->setValue(v); }

        str = settings.value(QString("/PlayVol/%1").arg(n+1)).toString();
        v = str.toInt(&ok);
        if (ok) pa_data[n].PlayVol = v;

        str = settings.value(QString("/levelOut/%1").arg(n+1)).toString();
        v = str.toInt(&ok);
        if (ok) L_Out[n]->setValue(v);

        str = settings.value(QString("/reverbType/%1").arg(n+1)).toString();
        v = str.toInt(&ok);
        if (ok) ReverbSelect[n]->setCurrentIndex(v);

        str = settings.value(QString("/MixWindowPosX/%1").arg(n+1)).toString();
        int x = str.toInt(&ok);
        if (ok) {
            str = settings.value(QString("/MixWindowPosY/%1").arg(n+1)).toString();
            int y = str.toInt(&ok);
            if (ok && (x != 0) && (y != 0)) {
                mixSetup[n]->move(QPoint(x, y));
                mixAlreadyShowed[n] = true; }
            }

        bool M = settings.value(QString("/MixEnabled/%1").arg(n+1)).toBool();
        if (M) { pa_data[n].mixEnabled = true; }
        if (pa_data[n].mixEnabled) setStyleSheet(Mix[n], mixOn); else setStyleSheet(Mix[n], mixOff);

        bool R = settings.value(QString("/ReverbOnly/%1").arg(n+1)).toBool();
        if (R) { reverbOnly[n]->setCheckState(Qt::Checked); pa_data[n].reverbOnly = true; }

        updateReverb(ReverbSelect[n]); }

    for (int n=0; n<fileDetailsMax; n++) {
        QString str = settings.value(QString("/FileDetail/FileName_%1").arg(n)).toString();
        if (!str.isEmpty()) {
            QString B = settings.value(QString("/FileDetail/loopBegin_%1").arg(n)).toString();
            QString E = settings.value(QString("/FileDetail/loopEnd_%1").arg(n)).toString();
            int S = settings.value(QString("/FileDetail/Sort_%1").arg(n)).toInt();
                fileDetails[n].fileName = str;
                fileDetails[n].loop_begin = QTime::fromString(B, "HH:mm:ss:zzz");
                fileDetails[n].loop_end = QTime::fromString(E, "HH:mm:ss:zzz");
                fileDetails[n].sort = S;
                disconnect(ui->comboBoxRecentFiles, SIGNAL(currentIndexChanged(int)), this, SLOT(recentListSelect(int)));
                ui->comboBoxRecentFiles->addItem(str);
                connect(ui->comboBoxRecentFiles, SIGNAL(currentIndexChanged(int)), this, SLOT(recentListSelect(int)));
            } }
    QString lastFile = settings.value( QString::fromUtf8("/file/last")).toString();
    if (!lastFile.isEmpty()) {
        ui->groupBox->setToolTip(lastFile);
        setFile(lastFile);
    }
    QString downloadSite = settings.value( QString::fromUtf8("/downloadsite")).toString();
    if (!downloadSite.isEmpty()) ui->lineEditDownloadSite->setText(downloadSite);
    QString uploadSite = settings.value( QString::fromUtf8("/uploadsite")).toString();
    if (!uploadSite.isEmpty()) ui->lineEditUploadSite->setText(uploadSite);
    QString uploadLogin = settings.value( QString::fromUtf8("/uploadlogin")).toString();
    if (!uploadLogin.isEmpty()) ui->lineEditFTPLogin->setText(uploadLogin);
    QString uploadPwd = settings.value( QString::fromUtf8("/uploadpwd")).toString();
    if (!uploadPwd.isEmpty()) pwd->setText(uploadPwd);
}



void MainWindow::saveConfig()
{
    QSettings settings("Remyxer.ini", QSettings::IniFormat);
    settings.setValue(QString::fromUtf8("/nbInstru"), nbInstru );
    settings.setValue(QString::fromUtf8("/MixEnabled"), mixEnabled);
    settings.setValue(QString::fromUtf8("/Latency"), ui->spinBoxLatency->value());
    settings.setValue(QString::fromUtf8("/FrameSize"), ui->spinBoxFrameSize->value());
    settings.setValue(QString::fromUtf8("/44k"), ui->radioButton44->isChecked() );
    settings.setValue(QString::fromUtf8("/geometry/width"), width() );
    settings.setValue(QString::fromUtf8("/geometry/height"), height() );
    settings.setValue(QString::fromUtf8("/geometry/posx"), x() );
    settings.setValue(QString::fromUtf8("/geometry/posy"), y() );
    for (int n=0; n<nbInstru; n++) {
        settings.setValue(QString("/nom/%1").arg(n+1), Nom[n]->text());
        if (Ecouteur[n]->currentIndex() == -1) {
            if (deviceName.contains(EcouteurSaved[n])) settings.setValue(QString("/ecouteur/%1").arg(n+1), "");
        else settings.setValue(QString("/ecouteur/%1").arg(n+1), EcouteurSaved[n]); }
        else settings.setValue(QString("/ecouteur/%1").arg(n+1), Ecouteur[n]->currentText());
        if (Micro[n]->currentIndex() == -1) {
            if (deviceMicName.contains(MicroSaved[n])) settings.setValue(QString("/micro/%1").arg(n+1), "");
        else settings.setValue(QString("/micro/%1").arg(n+1), MicroSaved[n]); }
        else settings.setValue(QString("/micro/%1").arg(n+1), Micro[n]->currentText());
        settings.setValue(QString("/ListenVol/%1").arg(n+1), pa_data[n].ListenVol);
        settings.setValue(QString("/RecVol/%1").arg(n+1), pa_data[n].RecVol);
        settings.setValue(QString("/RevLev/%1").arg(n+1), pa_data[n].RevLev);
        settings.setValue(QString("/PlayVol/%1").arg(n+1), pa_data[n].PlayVol);
        settings.setValue(QString("/levelOut/%1").arg(n+1), L_Out[n]->value());
        settings.setValue(QString("/reverbType/%1").arg(n+1), ReverbSelect[n]->currentIndex());
        settings.setValue(QString("/MixEnabled/%1").arg(n+1), pa_data[n].mixEnabled);
        settings.setValue(QString("/MixWindowPosX/%1").arg(n+1), mixSetup[n]->geometry().x());
        settings.setValue(QString("/MixWindowPosY/%1").arg(n+1), mixSetup[n]->geometry().y());
        settings.setValue(QString("/ReverbOnly/%1").arg(n+1), pa_data[n].reverbOnly);
        for (int i=0; i<nbInstru; i++) {
            QString name = Nom[n]->text();
            settings.setValue(QString("/mix_" + name + "/%1").arg(i+1), MixSend[n][i]->value());  }
    }
    for (int n=0; n<fileDetailsMax; n++) {
        if (!fileDetails[n].fileName.isEmpty()) {
            settings.setValue(QString("/FileDetail/FileName_%1").arg(n), fileDetails[n].fileName);
            settings.setValue(QString("/FileDetail/loopBegin_%1").arg(n), fileDetails[n].loop_begin.toString("HH:mm:ss:zzz"));
            settings.setValue(QString("/FileDetail/loopEnd_%1").arg(n), fileDetails[n].loop_end.toString("HH:mm:ss:zzz"));
            settings.setValue(QString("/FileDetail/Sort_%1").arg(n), fileDetails[n].sort);
        }
    }
    if (!fileName.isEmpty()) settings.setValue(QString::fromUtf8("/file/last"), fileName);
    settings.setValue(QString::fromUtf8("/downloadsite"), ui->lineEditDownloadSite->text());
    settings.setValue(QString::fromUtf8("/uploadsite"), ui->lineEditUploadSite->text());
    settings.setValue(QString::fromUtf8("/uploadlogin"), ui->lineEditFTPLogin->text());
    settings.setValue(QString::fromUtf8("/uploadpwd"), pwd->text());
    settings.setValue(QString::fromUtf8("/audiodriver"), ui->comboBoxDriver->currentText());
}





void MainWindow::chooseFile()
{
    setFocus();
    QString name = QFileDialog::getOpenFileName(this, tr("Open files"),  QString(QDir(fileName).path()), tr("Audio Files (*.wav)"));
    if (!name.isEmpty()) setFile(name);
}



int MainWindow::Callback(const void *input,
             void *output,
             unsigned long frameCount,
             const PaStreamCallbackTimeInfo* paTimeInfo,
             PaStreamCallbackFlags statusFlags,
             void *userData)
 {
    (void) paTimeInfo; /* Prevent unused variable warnings. */
    (void) statusFlags;
    (void) input;

    pa_Data *data = (pa_Data *)userData; /* we passed a data structure into the callback so we have something to work with */
    data->runing = true;


    float *cursor; /* current pointer into the output  */
    float *in = (float *)input;
    float *out = (float *)output;
    quint64 intFrameCount = frameCount;
    quint64 thisRead;
    cursor = out; /* set the output cursor to the beginning */

    /* are we going to read past the end of the file?*/
    if (intFrameCount > (data->frames - data->position))
    {
        /*if we are, only read to the end of the file*/
        thisRead = data->frames - data->position;
        /* and then loop to the beginning of the file */
        data->position = 0;
        Paused = true;
        theEnd = true;
        //return paComplete;
    }
    else
    {
        /* otherwise, we'll just fill up the rest of the output buffer */
        thisRead = intFrameCount;
        /* and increment the file position */
        if (!Paused) data->position += thisRead;
    }

    // remove old mix frames
    while (data->mix_del_list.count() > 100) { delete data->mix_del_list.takeFirst(); }

    // get input buffer and copy it to output in Micro and Recording mode
    if (Paused) {
        for (quint64 i = 0; i < thisRead; i++) {
            // Copy input to ouput
            data->buffer_in[i].L = *in * data->G_In;
            data->buffer_in[i].R = *in++ * data->G_In;
    } }
    else if ((data->mode != Playback) && (data->mode != trackOff)) {
        // set recorded flag
        if ((data->mode == Record) && Recording) data->recorded = true;
        for (quint64 i = 0; i < thisRead; i++) {
            // Copy input to ouput
            data->buffer_in[i].L = *in * data->G_In;
            data->buffer_in[i].R = *in++ * data->G_In;
            // Copy input to wav array
            if ((data->mode == Record) && Recording) {
            if ((loopRecord && (data->position > loop_begin) && (data->position < loop_end)) || !loopRecord) {
            data->wavRecord[(data->position << 1) + (i << 1)] = data->buffer_in[i].L * data->G_Rec;
            data->wavRecord[(data->position << 1) + (i << 1) + 1] = data->buffer_in[i].R * data->G_Rec; }
            }
            // add playback from other tracks
            // not needed any more since track inter mix
            //if (data->mixEnabled && mixEnabled && (!Paused)) {
            //change 15/09/2022
            //if (mixEnabled && (!Paused)) {
            if (mixEnabled && data->mixEnabled) {
            for (int n=0; n<nbInstru; n++) {
                float g = 0;
                if (n == data->myIndex) g = 0; //pa_data[n].G_Play;
                if ((pa_data[n].wavRecord != nullptr) && (pa_data[n].mode == Playback) && (n != data->myIndex) && pa_data[n].mixEnabled) g = pa_data[n].mixGain[data->myIndex];
                if (g > 0) {
                    data->buffer_in[i].L += pa_data[n].wavRecord[(data->position << 1) + (i << 1)] * g;
                    data->buffer_in[i].R += pa_data[n].wavRecord[(data->position << 1) + (i << 1) + 1] * g;
                } } }
        } }
    else {
        // Here it is when playback mode is selected
        // playback if wav of other tracks if wav was recorded
        for (quint64 i = 0; i < thisRead; i++) {
            data->buffer_in[i].L = 0;
            data->buffer_in[i].R = 0;
            // add playback from All tracks
            for (int n=0; n<nbInstru; n++) {
                if ((mixEnabled || (n == data->myIndex)) && (pa_data[n].wavRecord != nullptr) && (pa_data[n].mode == Playback) && (pa_data[n].mixEnabled || (n == data->myIndex))) {
                    float g = pa_data[n].mixGain[data->myIndex];
                    if (n == data->myIndex) g = pa_data[n].G_Play;
                    data->buffer_in[i].L += pa_data[n].wavRecord[(data->position << 1) + (i << 1)] * g;
                    data->buffer_in[i].R += pa_data[n].wavRecord[(data->position << 1) + (i << 1) + 1] * g; } }
        } }

    // process reverb to input buffer
    if (data->mode != trackOff) sf_reverb_process(data->rv, intFrameCount , data->buffer_in, data->buffer_out);

    // Create new frames and copy one new frame to each other tracks with apropriate gain
    // if mix enabled copy input buffer to all other output mix matrix with its specific gain
    if (mixEnabled && data->mixEnabled) {
    for (int n = 0; n < nbInstru; n++) {
        if ((n != data->myIndex) && (data->mode != trackOff)) {
            if (pa_data[n].runing) {
                if (mix_track_Hor_Vert[data->myIndex][n].mix_send_list.count() < 200) {
                mix_frame *mix = new mix_frame;
                for (quint64 i = 0; i < thisRead; i++) {
                    mix->frame[i].L = data->buffer_in[i].L * data->mixGain[n];
                    mix->frame[i].R = data->buffer_in[i].R * data->mixGain[n];
                }
                data->mix_del_list.append(mix);
                mix_track_Hor_Vert[data->myIndex][n].mix_send_list.append(mix);
            } else qDebug() << "mix overflow";  } } } }

    if (Paused) {
        // Get input buffer
        if (data->mode == trackOff) {
            for (quint64 i = 0; i < thisRead; i++) {
            *cursor++ = 0;
            *cursor++ = 0; }
        } else {
            for (quint64 i = 0; i < thisRead; i++) {
                float L = 0;
                float R = 0;
                if (!data->reverbOnly) {
                L = data->buffer_in[i].L;
                R = data->buffer_in[i].R; }
            // Mix all other tracks to hear others even when paused
            for (int n = 0; n < nbInstru; n++) {
                if (n != data->myIndex) {
                    if (!mix_track_Hor_Vert[n][data->myIndex].mix_send_list.isEmpty()) {
                    L += mix_track_Hor_Vert[n][data->myIndex].mix_send_list.first()->frame[i].L;
                    R += mix_track_Hor_Vert[n][data->myIndex].mix_send_list.first()->frame[i].R; } } }
            // set vumeter level
                if (data->buffer_in[i].L > data->levelLeft) data->levelLeft = data->buffer_in[i].L;
                if (data->buffer_in[i].R > data->levelRight) data->levelRight = data->buffer_in[i].R;
                if (data->reverb) {
                    *cursor++ = (L + (data->buffer_out[i].L * data->G_RevLev));
                    *cursor++ = (R + (data->buffer_out[i].R * data->G_RevLev));
                }
                else { *cursor++ = L; *cursor++ = R; }
            }
        }
        for (int n = 0; n < nbInstru; n++) {
            if (n != data->myIndex) {
                if (!mix_track_Hor_Vert[n][data->myIndex].mix_send_list.isEmpty()) {
        mix_track_Hor_Vert[n][data->myIndex].mix_send_list.takeFirst(); } } }
        // remove one frames of each track if available
        for (int n = 0; n < nbInstru; n++) {
            //if (n != data->myIndex) {
                while (mix_track_Hor_Vert[n][data->myIndex].mix_send_list.count() > 100) mix_track_Hor_Vert[n][data->myIndex].mix_send_list.removeFirst(); }
        return paContinue;
    }

    // add wav file to output
    if (data->mode == trackOff) {
        for (quint64 i = 0; i < thisRead; i++) {
        *cursor++ = 0;
        *cursor++ = 0; }
    } else {
    for (quint64 i = 0; i < thisRead; i++) {
            float L = wav_st[data->position + i].L * data->G_Wav * !muteState;
            if (data->reverb) L += (data->buffer_in[i].L + data->buffer_out[i].L); else if (!data->reverbOnly) L += data->buffer_in[i].L * data->G_RevLev;
            float R = wav_st[data->position + i].R * data->G_Wav * !muteState;
            if (data->reverb) R += (data->buffer_in[i].R + data->buffer_out[i].R); else if (!data->reverbOnly) R += data->buffer_in[i].R * data->G_RevLev;
        // Mix all other tracks
        for (int n = 0; n < nbInstru; n++) {
            if (n != data->myIndex) {
                if (!mix_track_Hor_Vert[n][data->myIndex].mix_send_list.isEmpty()) {
                L += mix_track_Hor_Vert[n][data->myIndex].mix_send_list.first()->frame[i].L;
                R += mix_track_Hor_Vert[n][data->myIndex].mix_send_list.first()->frame[i].R;
                } } }
            if (L > data->levelLeft) data->levelLeft = L;
            if (R > data->levelRight) data->levelRight = R;
            *cursor++ = L;
            *cursor++ = R;
    } }
    for (int n = 0; n < nbInstru; n++) {
        if (n != data->myIndex) {
            if (!mix_track_Hor_Vert[n][data->myIndex].mix_send_list.isEmpty()) {
                //qDebug() << "remove fisrt";
    mix_track_Hor_Vert[n][data->myIndex].mix_send_list.takeFirst(); } } }

    //qDebug() << paTimeInfo->outputBufferDacTime; //QString("%1  %2").arg(thisRead).arg(data->position);
    if ((loop) && (loop_end > loop_begin)) {
        if (data->position < loop_begin) data->position = loop_begin;
        else if (data->position > loop_end) data->position = loop_begin;
    }
    // remove one frames of each track if available
    for (int n = 0; n < nbInstru; n++) {
            while (mix_track_Hor_Vert[n][data->myIndex].mix_send_list.count() > 100) mix_track_Hor_Vert[n][data->myIndex].mix_send_list.removeFirst();
    }
    return paContinue;
}





/*
* This routine is called by portaudio when playback is done.
*/
static void StreamFinished(void *)
{
    //pa_Data *data = (pa_Data *)userData; /* we passed a data structure into the callback so we have something to work with */
    qDebug() << "Stream Completed";
    finished = true;
}


void MainWindow::rewind()
{
    setFocus();
    if (Runing) {
        if (ui->checkBoxLoop->isChecked()) {
            for (int n=0; n<nbInstru; n++) { pa_data[n].position = loop_begin; } }
        else {
            for (int n=0; n<nbInstru; n++) { pa_data[n].position = 0; } }
        }
    else  {
        stop();
    }
}


void MainWindow::play()
{
    if (fileName.isEmpty())
    {
        QMessageBox msgBox;
        msgBox.setText("No file selected");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
        ui->buttonPlay->setChecked(false);
        return; }
    setFocus();
    if (!Runing) { return; }
    if (!Recording) Paused = !Paused;
    if (Paused) {
        Recording = false;
        ui->buttonPlay->setChecked(false);
        ui->buttonRecord->setChecked(false);
        bool rec = false;
        for (int n=0; n<nbInstru; n++) { if (pa_data[n].recorded) {
                SaveTrack[n]->setEnabled(true);
                FtpUpload[n]->setEnabled(true);
                rec = true; }
            }
        ui->buttonSave->setEnabled(rec);
    }
    else
    {
            Recording = false;
            ui->buttonPlay->setChecked(true);
            ui->buttonRecord->setChecked(false);
    }
}


void MainWindow::record()
{
    setFocus();
    if (!Runing) { return; }
    if ((!Paused && Recording) || Paused) Paused = !Paused;
    if (Paused) {
        Recording = false;
        ui->buttonPlay->setChecked(false);
        ui->buttonRecord->setChecked(false);
        bool rec = false;
        for (int n=0; n<nbInstru; n++) { if (pa_data[n].recorded) { rec = true;
            SaveTrack[n]->setEnabled(true);
            FtpUpload[n]->setEnabled(true); }
        }
        ui->buttonSave->setEnabled(rec);
    }
    else
    {
            Recording = true;
            ui->buttonPlay->setChecked(false);
            ui->buttonRecord->setChecked(true);
            for (int n=0; n<nbInstru; n++) {
                SaveTrack[n]->setEnabled(false);
                FtpUpload[n]->setEnabled(false); }
            ui->buttonSave->setEnabled(false);
    }
}


void MainWindow::ONOFF()
{
    bool none = true;
    firstDevice = -1;
    ui->wavSlider->setValue(0);
    for (int n=0; n<nbInstru; n++) {
        pa_data[n].position = 0;
        if (Ecouteur[n]->currentIndex() != -1) none = false;
        else {
            //if (pa_data[n].mode != Playback)
            //pa_data[n].mode = trackOff;
            updateReplay(); }
        // search for fisrt device running for delay calculation
        if ((firstDevice == -1) && (Ecouteur[n]->currentIndex() != -1)) firstDevice = n;
        delayMean[n] = 0;
    }
    if (none) return;
    turnedON = !turnedON;
    if (turnedON) {
        ui->buttonONOFF->setIcon(QPixmap(":/images/ON"));
        ON();
    }
    else
    {
           ui->buttonONOFF->setIcon(QPixmap(":/images/OFF"));
           OFF();
       }
}


void MainWindow::ON()
{
    ui->buttonRewind->setEnabled(true);
    ui->buttonPlay->setEnabled(true);
    ui->buttonRecord->setEnabled(true);
    ui->buttonHP->setChecked(false);
    ui->buttonHP->setEnabled(true);
    bool rec = false;
    for (int n=0; n<nbInstru; n++) {
#if defined(Q_OS_WIN)
        Micro[n]->setEnabled(false);
#endif
        Ecouteur[n]->setEnabled(false);
        if (pa_data[n].mode == Record) rec = true;
    }
    ui->buttonRecord->setEnabled(rec);
    finished = false;
    setFocus();
    ui->buttonRecord->setChecked(false);
    bool none = true;
    for (int n=0; n<nbInstru; n++) {
        if (Ecouteur[n]->currentIndex() != -1) none = false;
        Nom[n]->setReadOnly(true);
        SaveTrack[n]->setEnabled(false);
        FtpUpload[n]->setEnabled(false);
    }
    if (none) return;
    ui->buttonLoad->setEnabled(false);
    ui->comboBoxRecentFiles->setEnabled(false);
    ui->buttonSave->setEnabled(false);
    ui->radioButton44->setEnabled(false);
    ui->radioButton48->setEnabled(false);
    ui->spinBoxLatency->setEnabled(false);
    ui->spinBoxFrameSize->setEnabled(false);
    ui->buttonSearchDevice->setEnabled(false);
    ui->buttonMetronome->setEnabled(false);
    ui->buttonWebUpdate->setEnabled(false);
    ui->comboBoxDriver->setEnabled(false);
    ui->buttonSave->setEnabled(false);
    Paused = true;
    playNow();
}


void MainWindow::OFF()
{
    stop();
    ui->buttonSave->setChecked(false);
    ui->buttonSave->setEnabled(false);

    ui->buttonHP->setChecked(false);
    ui->buttonHP->setEnabled(false);

    ui->buttonLoad->setEnabled(true);
    ui->buttonRewind->setEnabled(false);
    ui->buttonPlay->setEnabled(false);
    ui->buttonRecord->setEnabled(false);
    ui->buttonSearchDevice->setEnabled(true);
    ui->buttonMetronome->setEnabled(true);
    ui->buttonWebUpdate->setEnabled(true);
    ui->comboBoxDriver->setEnabled(true);
    for (int n=0; n<nbInstru; n++) {
#if defined(Q_OS_WIN)
        Micro[n]->setEnabled(true);
#endif
        Ecouteur[n]->setEnabled(true); }
    bool rec = false;
    for (int n=0; n<nbInstru; n++) {
        if (pa_data[n].recorded) {
            rec = true;
            SaveTrack[n]->setEnabled(true);
            FtpUpload[n]->setEnabled(true); }
        Nom[n]->setReadOnly(false);
        vuMeter[n]->setEnabled(false);
    }
    ui->buttonSave->setEnabled(rec);

}



void MainWindow::sliderMoved(int position)
{
    int c = position;
    int s = c / 100;
    int m = s / 60;
    ui->labelTime->setText(QString("%1:%2:%3").arg(m, 2, 10, QChar('0')).arg(s%60, 2, 10, QChar('0')).arg(c%100, 2, 10, QChar('0')));
}


void MainWindow::sampleRateChange()
{
    if (ui->radioButton48->isChecked()) SAMPLE_RATE = 48000; else SAMPLE_RATE = 44100;
    ui->log->append(QString("Sample Rate is %1").arg(SAMPLE_RATE));
    setFile(fileName);
}


void MainWindow::recentListSelect(int index)
{
    if (index != -1) setFile(ui->comboBoxRecentFiles->currentText());
}


void MainWindow::initToggle()
{
    initEnabled = !initEnabled;
    ui->buttonInit->setChecked(initEnabled);
}


void MainWindow::frameSizeChanged(int s)
{
    FRAME_SIZE = s;
}


void MainWindow::driverChanged(int driver)
{
    if (driver < 0) return;
#if defined(Q_OS_LINUX)
    for (int n=0; n<nbInstru; n++) {
        QString currentDevice = Ecouteur[n]->currentText();
        int Po = currentDevice.indexOf("(");
        int Pf = currentDevice.indexOf(")");
        QString device;
        if ((Po > 0) && (Pf > 0)) device = currentDevice.mid(Po + 1, Pf-Po-1);
        disconnect(Ecouteur[n], 0, 0, 0);
        Ecouteur[n]->clear();
        for (int i=0; i<deviceName.count(); i++) {
            if (deviceAPI.at(i) == driver) Ecouteur[n]->addItem(deviceName.at(i));
        }
        Ecouteur[n]->setCurrentIndex(-1);
        connect(Ecouteur[n], &myCombo::currentTextChanged, [=] { updateEcouteur( Ecouteur[n] );  } );
        if (!device.isEmpty()) {
            int index = -1;
            for (int c=0; c<Ecouteur[n]->count(); c++) {
                qDebug() << Ecouteur[n]->itemText(c) + " = " + device;
                if (Ecouteur[n]->itemText(c).contains(device)) index = c;
            }
            if (index != -1) {
                Ecouteur[n]->setCurrentIndex(index);
            }
        }
    }
#endif
#if defined(Q_OS_WIN)
    for (int n=0; n<nbInstru; n++) {
        QString currentDevice = Ecouteur[n]->currentText();
        int Po = currentDevice.indexOf("(");
        int Pf = currentDevice.indexOf(")");
        QString device;
        if ((Po > 0) && (Pf > 0)) device = currentDevice.mid(Po + 1, Pf-Po-1);
        disconnect(Ecouteur[n], 0, 0, 0);
        disconnect(Micro[n], 0, 0, 0);
        Ecouteur[n]->clear();
        for (int i=0; i<deviceName.count(); i++) {
            if (deviceAPI.at(i) == driver) Ecouteur[n]->addItem(deviceName.at(i));
        }
        Ecouteur[n]->setCurrentIndex(-1);
        Micro[n]->clear();
        for (int i=0; i<deviceName.count(); i++) {
            if (deviceMicAPI.at(i) == driver) Micro[n]->addItem(deviceMicName.at(i));
        }
        Micro[n]->setCurrentIndex(-1);
        connect(Micro[n], &myCombo::currentTextChanged, [=] { updateMicro( Micro[n] );  } );
        connect(Ecouteur[n], &myCombo::currentTextChanged, [=] { updateEcouteur( Ecouteur[n] );  } );
        if (!device.isEmpty()) {
            int index = -1;
            for (int c=0; c<Ecouteur[n]->count(); c++) {
                qDebug() << Ecouteur[n]->itemText(c) + " = " + device;
                if (Ecouteur[n]->itemText(c).contains(device)) index = c;
            }
            if (index != -1) {
                qDebug() << device;
                Ecouteur[n]->setCurrentIndex(index);
            }
        }
    }
#endif
}


void MainWindow::vuMeterToggle()
{
    vuMeterEnable = !vuMeterEnable;
    if (vuMeterEnable) {
        ui->buttonVuMeter->setIcon(QIcon(":/images/vu-meter-on.png"));
        for (int n=0; n<nbInstru; n++) {
        vuMeter[n]->show();
        }
    }
    else
    {
        ui->buttonVuMeter->setIcon(QIcon(":/images/vu-meter-off.png"));
        for (int n=0; n<nbInstru; n++) {
        vuMeter[n]->setLeftValue(0);
        vuMeter[n]->setRightValue(0);
        vuMeter[n]->hide();
        }
    }
}

void MainWindow::logToggle()
{
    if (ui->log->isHidden()) {
        ui->log->show();
        ui->lineEditDownloadSite->show();
        ui->labelDownloadSite->show();
        ui->lineEditUploadSite->show();
        ui->labelUploadSite->show();
        ui->comboBoxDriver->show();
        ui->labelFTPLogin->show();
        ui->labelFTPpwd->show();
        ui->lineEditFTPLogin->show();
        pwd->show();
        ui->spinBoxLatency->show();
        ui->labelLatency->show();
        ui->spinBoxLatency->show();
        ui->labelLatency->show();
        ui->labelFrameSize->show();
        ui->spinBoxFrameSize->show();
    }
    else
    {
        ui->log->hide();
        ui->lineEditDownloadSite->hide();
        ui->labelDownloadSite->hide();
        ui->lineEditUploadSite->hide();
        ui->labelUploadSite->hide();
        ui->comboBoxDriver->hide();
        ui->labelFTPLogin->hide();
        ui->labelFTPpwd->hide();
        ui->lineEditFTPLogin->hide();
        pwd->hide();
        ui->spinBoxLatency->hide();
        ui->labelLatency->hide();
        ui->labelFrameSize->hide();
        ui->spinBoxFrameSize->hide();
    }
}



void MainWindow::mixToggle()
{
    if (QGuiApplication::keyboardModifiers().testFlag(Qt::ControlModifier)) {
        bool allTrue = true;
        for (int n=0; n<nbInstru; n++) { if (!pa_data[n].mixEnabled) allTrue = false; }
        ui->buttonMix->setChecked(allTrue);
        if (allTrue) {
            for (int n=0; n<nbInstru; n++) { pa_data[n].mixEnabled = false; setStyleSheet(Mix[n], mixOff); }
            setStyleSheet(ui->buttonMix, mixOff);
            ui->buttonMix->setToolTip("Mixer OFF");
        }
        else {
            for (int n=0; n<nbInstru; n++) { pa_data[n].mixEnabled = true; setStyleSheet(Mix[n], mixOn); }
            setStyleSheet(ui->buttonMix, mixOn);
            ui->buttonMix->setToolTip("Mixer ON");
        }
    }
    else {
        mixEnabled = !mixEnabled;
        if (mixEnabled) {
            setStyleSheet(ui->buttonMix, mixOn);
            ui->buttonMix->setToolTip("Mixer ON");
        }
        else {
            setStyleSheet(ui->buttonMix, mixOff);
            ui->buttonMix->setToolTip("Mixer OFF");
        }
    }
}


void MainWindow::loopRecordChanged(int)
{
    if (ui->checkBoxLoopRecord->isChecked()) { if (loop_end > loop_begin) loopRecord = true; }
    else { loopRecord = false; }
    setFocus();
}



void MainWindow::reverbOnlyChanged(int)
{
    for (int n=0; n<nbInstru; n++) {
        if (reverbOnly[n]->isChecked()) pa_data[n].reverbOnly = true;
        else pa_data[n].reverbOnly =false; }
}


void MainWindow::loopEnable(int)
{
    if (ui->checkBoxLoop->isChecked()) { if (loop_end > loop_begin) loop = true; }
    else {
        loop = false; }
    setFocus();
}

void MainWindow::stopSlider()
{
    sliderMove = false;
}



void MainWindow::movePosition()
{
    setFocus();
    int position = ui->wavSlider->value();
    for (int n=0; n<nbInstru; n++) {
        pa_data[n].position = position * (SAMPLE_RATE / 100);
    }
    sliderMove = true;
}


int64_t MainWindow::loadWavFile(QString fileName, float *&wavSound, bool &converted)
{
    converted = true;
    int64_t wavSize = 0;
    SF_INFO sfInfo;
    SNDFILE *sndFile = sf_open(fileName.toLocal8Bit(), SFM_READ, &sfInfo);
    if (!sndFile) { ui->log->append(fileName + " file not found"); return 0; }
    int64_t frames = sfInfo.frames;
    if (sfInfo.channels != 2) {
        QMessageBox msgBox;
        msgBox.setText("File is not stereo\nCan't load file");
        msgBox.setStandardButtons(QMessageBox::Abort);
        msgBox.exec();
        return 0;
    }
    if (sfInfo.samplerate == SAMPLE_RATE)
    {
        converted = false;
        if (wavSound) delete[] wavSound;
        wavSound = new float[frames * 2];
        if (!wavSound) { ui->log->append(fileName + " file not loaded, not enough memory"); return 0; }
        sf_seek(sndFile, 0, SEEK_SET);
        sf_readf_float(sndFile, wavSound, frames);
        //qDebug() << QString("Channels = %1").arg(sfInfo.channels);
        //qDebug() << QString("Frames = %1").arg(sfInfo.frames);
        wavSize = frames;
        ui->log->append(fileName + QString(" file loaded in %1").arg(SAMPLE_RATE));
    }
    else if ((sfInfo.samplerate == 44100) && (SAMPLE_RATE == 48000))
    {
        float *wav = new float[frames * 2];
        if (!wav) { ui->log->append(fileName + " file not loaded, not enough memory"); return 0; }
        sf_seek(sndFile, 0, SEEK_SET);
        sf_readf_float(sndFile, wav, frames);
        wavSize = frames;
        ui->log->append("Convert " + fileName + "from 44100 to 48000");
        uint32_t last_pos = frames - 1;
        uint32_t dstSize = (uint32_t) (frames * ((float) SAMPLE_RATE / 44100));
        if (wavSound) delete[] wavSound;
        wavSound = new float[dstSize * 2];
        if (!wavSound) { ui->log->append(fileName + " file not loaded, not enough memory"); return 0; }
        wavSize = dstSize;
        for (uint32_t idx = 0; idx < dstSize; idx++) {
               float index = ((float) idx * 44100) / (SAMPLE_RATE);
               uint32_t p1 = (uint32_t) index;
               float coef = index - p1;
               uint32_t p2 = (p1 == last_pos) ? last_pos : p1 + 1;
               wavSound[idx << 1] = ((1.0f - coef) * wav[p1 << 1] + coef * wav[p2 << 1]);
               wavSound[(idx << 1) + 1] = ((1.0f - coef) * wav[(p1 << 1) + 1] + coef * wav[(p2 << 1) + 1]);
        }
       ui->log->append(fileName + " file loaded and converted from 44100 to 48000");
       delete[] wav;
    }
    else if ((sfInfo.samplerate == 48000) && (SAMPLE_RATE == 44100)) {
        float *wav = new float[frames << 1];
        if (!wav) { ui->log->append(fileName + " file not loaded, not enough memory"); return 0; }
        sf_seek(sndFile, 0, SEEK_SET);
        sf_readf_float(sndFile, wav, frames);
        wavSize = frames;
        ui->log->append("Convert from 48000 to 44100");
        uint32_t last_pos = frames - 1;
        uint32_t dstSize = (uint32_t) (frames * ((float) SAMPLE_RATE / 48000));
        if (wavSound) delete[] wavSound;
        wavSound = new float[dstSize << 1];
        if (!wavSound) { ui->log->append(fileName + " file not loaded, not enough memory"); return 0; }
        wavSize = dstSize;
        for (uint32_t idx = 0; idx < dstSize; idx++) {
               float index = ((float) idx * 48000) / (SAMPLE_RATE);
               uint32_t p1 = (uint32_t) index;
               float coef = index - p1;
               uint32_t p2 = (p1 == last_pos) ? last_pos : p1 + 1;
               wavSound[idx << 1] = ((1.0f - coef) * wav[p1 << 1] + coef * wav[p2 << 1]);
               wavSound[(idx << 1) + 1] = ((1.0f - coef) * wav[(p1 << 1) + 1] + coef * wav[(p2 << 1) + 1]);
        }
       ui->log->append(fileName + " file loaded and converted from 44100 to 48000");
       delete[] wav;
    }
    else { ui->log->append("Not compatible file format for " + fileName); }
    sf_close(sndFile);
    return wavSize;
}



void MainWindow::gen_metronome()
{
    bool ok;
    int bpm = QInputDialog::getInt(this, tr("BPM"), tr("BPM :"), 120, 50, 280, 1, &ok);
    if (!ok) return;
    int t = QInputDialog::getInt(this, tr("Durée"), tr("Durée en secondes :"), 300, 1, 3600, 1, &ok);
    if (!ok) return;
tryAgain:
    QString ticFileName = QFileDialog::getOpenFileName(this, ("Open File"), "", ("Wav (*.wav )"));
    if (ticFileName.isEmpty()) return;
    float *wavTicSound = nullptr;
    bool converted = false;
    int64_t ticSize = loadWavFile(ticFileName, wavTicSound, converted);
    if (wavTicSound == nullptr) {
        QMessageBox msgBox;
        msgBox.setText("Can't load file");
        msgBox.setStandardButtons(QMessageBox::Abort);
        msgBox.exec();
        return; }
    if (ticSize > (SAMPLE_RATE / 2)) {
        QMessageBox msgBox;
        msgBox.setText("File is too long");
        msgBox.setInformativeText("You must choose a shorter file");
        msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Cancel);
        if (msgBox.exec() == QMessageBox::Cancel) return;
        goto tryAgain; }
nameAgain:
    QString text = QInputDialog::getText(this, tr("Metronome sound file"), tr("Choose a sound file :"), QLineEdit::Normal, "Sound file", &ok);
    if (!ok) return;
    QString wavFileName = text + ".wav"; // + QDir::separator() + newfName;
    if (QFile::exists(wavFileName)) {
        QMessageBox msgBox;
        msgBox.setText("File already exists");
        msgBox.setInformativeText("Do you want to  overwrite ?");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No |QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Cancel);
        int r = msgBox.exec();
        if (r == QMessageBox::Cancel) return;
        if (r == QMessageBox::No) goto nameAgain;
        }
    int64_t wavSize = t * SAMPLE_RATE;
    float *wav_st = new float[wavSize * 2];
    int sampleClic = SAMPLE_RATE * 60 / bpm;
    for (int n=0; n<wavSize * 2; n++) { wav_st[n] = 0; }
    for (int n=0; n<wavSize * 2; n+=sampleClic * 2) {
        for (int i=0; i<ticSize; i+=1) {
            wav_st[n + (i << 1)] = wavTicSound[i << 1];
            wav_st[n + (i << 1) + 1] = wavTicSound[i << 1]; } }
    SF_INFO sfinfo ;
    sfinfo.channels = 2;
    sfinfo.samplerate = SAMPLE_RATE;
    sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
    SNDFILE *outfile = sf_open(wavFileName.toLocal8Bit(), SFM_WRITE, &sfinfo);
    if (!outfile) { ui->log->append("Error writing file " + wavFileName); return; }
    if (sf_write_float(outfile, wav_st, wavSize * 2) != (wavSize * 2)) ui->log->append(wavFileName + " could not be saved");
    sf_write_sync(outfile);
    sf_close(outfile);
    delete[] wav_st;
    setFile(wavFileName);
}


QString MainWindow::checkWavFiles(QString wavName)
{
    QFileInfo fInfo(fileName);
    QString fileName = fInfo.absolutePath() + QDir::separator() + recDir + QDir::separator() + wavName + ".wav";
    QString dirPath = fInfo.absolutePath() + QDir::separator() + recDir;
    //get directory file list
    QDir dir(dirPath);
    QList <QFileInfo> fileList = dir.entryInfoList();
    QList <QFileInfo> foundList;
    QList <QDate> dateList;
    foreach (QFileInfo info, fileList)
    {
        // get files starting with wavName
        if (info.fileName().startsWith(wavName))
        {
            QDate date = getFileDate(info.fileName());
            if (date.isValid()) {
                foundList.append(info);
                dateList.append(date); }
        }
    }
    if (foundList.isEmpty())
    {
        return fileName;
    }
    if (foundList.count() == 1)
    {
        QString fName = fInfo.absolutePath() + QDir::separator() + recDir + QDir::separator() + foundList.first().fileName();
        return fName;
    }
    QDate lastDate = dateList.first();
    QString lastFile = foundList.first().fileName();
    QString fName = fInfo.absolutePath() + QDir::separator() + recDir + QDir::separator() + foundList.at(0).fileName();
    for (int n=1; n<foundList.count(); n++)
    {
        if (lastDate.daysTo(dateList.at(n)) > 0) {
            fName = fInfo.absolutePath() + QDir::separator() + recDir + QDir::separator() + foundList.at(n).fileName();
            // Do you want to delete lastFile;
            lastFile = foundList.at(n).fileName();
        }
    }
    return fName;
}



void MainWindow::setFile(QString str)
{
    loop_begin = -1;
    loop_end = -1;
    ui->labelBegin->setText("Début : (F1)");
    ui->labelEnd->setText("Fin : (F2)");
    fileName = str;
    ui->groupBox->setToolTip(fileName);
    sndFile = sf_open(fileName.toUtf8(), SFM_READ, &sfInfo);

    float *wavDataFile = nullptr;
    bool converted = false;
    Frames = loadWavFile(fileName, wavDataFile, converted);
    if (wav_st) delete[] wav_st;
    wav_st = new sf_sample_st[Frames];
    if (!wav_st) {
        QMessageBox msgBox;
        msgBox.setText("Out of memory");
        msgBox.setStandardButtons(QMessageBox::Cancel);
        msgBox.exec();
        return;
    }
    for (int n=0; n<Frames; n++) {
        wav_st[n].L = wavDataFile[n << 1];
        wav_st[n].R = wavDataFile[(n << 1) + 1];
    }
    delete[] wavDataFile;
    sf_close(sndFile);
    //if (Frames < sfInfo.frames) {
    //    ui->log->append("file could not be loaded completly");
    //    return; }
    if (converted) {
        QMessageBox msgBox;
        msgBox.setText("File was converted\nFor better quality you should change sampling rate\nif output device is compatible");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec(); }
    if (sndFile == nullptr) return;
    int c = Frames / (SAMPLE_RATE / 100);
    int s = c / 100;
    int m = s / 60;
    ui->labelTime->setText(QString("%1:%2:%3").arg(m, 2, 10, QChar('0')).arg(s%60, 2, 10, QChar('0')).arg(c%100, 2, 10, QChar('0')));
    ui->wavSlider->setValue(0);
    ui->wavSlider->setMaximum(c);

    // look for wav recorded
    for (int n=0; n<nbInstru; n++) {
        pa_data[n].fileLoaded = false;
        QString wavFileName = checkWavFiles(getWavRecFileName(n)); // + ".wav";
        ui->log->append("look for " + wavFileName);
        if (Ecouteur[n]->currentIndex() == -1) pa_data[n].mode = trackOff;
        if (pa_data[n].wavRecord) delete [] pa_data[n].wavRecord;
        pa_data[n].wavRecord = nullptr;
            QFile wavFile(wavFileName);
            if (wavFile.exists())
            {
                bool converted = false;
                int64_t wavSize = loadWavFile(wavFileName, pa_data[n].wavRecord, converted);
                if (wavSize >= Frames) {
                    ui->log->append("file loaded " + wavFileName);
                    Nom[n]->setToolTip("file loaded " + wavFileName);
                    fileLoaded[n] = wavFileName;
                pa_data[n].fileLoaded = true;
                if (Ecouteur[n]->currentIndex() == -1) pa_data[n].mode = Playback;
                } else Nom[n]->setToolTip(QString("File size %1 doesn't match %2, file not loaded \n").arg(wavSize).arg(Frames) + wavFileName);
                } else Nom[n]->setToolTip("File not found \n" + wavFileName);
        if (pa_data[n].wavRecord == nullptr) { pa_data[n].wavRecord = new float[Frames * 2];
            if (!pa_data[n].wavRecord) {
                QMessageBox msgBox;
                msgBox.setText("Out of memory");
                msgBox.setStandardButtons(QMessageBox::Cancel);
                msgBox.exec();
                return; }
        for (int f=0; f<(Frames * 2); f++) { pa_data[n].wavRecord[f] = 0; } }
        if (pa_data[n].fileLoaded == true) setStyleSheet(Nom[n], loaded); else setStyleSheet(Nom[n], notLoaded);
    }
    updateReplay();
    // check file details (loop begin end recall)
    fileDetailsIndex = -1;
    for (int n=0; n<fileDetailsMax; n++) {
        if (fileDetails[n].fileName == fileName) {
            // Name found, get values
            //qDebug() << "Yoo";
            bool state = Runing;
            Runing = true;
            quint64 b = fileDetails[n].loop_begin.msecsSinceStartOfDay();
            setLoopBegin(b  * SAMPLE_RATE / 1000);
            quint64 e = fileDetails[n].loop_end.msecsSinceStartOfDay();
            setLoopEnd(e * SAMPLE_RATE / 1000);
            Runing = state;
            fileDetailsIndex = n;
            int sortValue = fileDetails[n].sort;
            // make chosen file as last used : sort = 1
            for (int i=0; i<fileDetailsMax; i++) { if (fileDetails[i].sort < sortValue) fileDetails[i].sort ++; }
            fileDetails[fileDetailsIndex].sort = 1;
            setRecentList();
            return;
        }
    }
    // else add info into empty place
    for (int n=0; n<fileDetailsMax; n++) {
        if (fileDetails[n].fileName.isEmpty()) {
            //qDebug() << "take empty one";
            fileDetails[n].fileName = fileName;
            fileDetailsIndex = n;
            for (int i=0; i<n; i++) { if (fileDetails[i].sort >= 0) fileDetails[i].sort ++; }
            fileDetails[n].sort = 1;
            setRecentList();
            return;
        }
    }
    // else remove oldest one and take this place for the new one
    for (int n=0; n<fileDetailsMax; n++) { fileDetails[n].sort ++; }
    //qDebug() << "full, remove one";
    for (int n=0; n<fileDetailsMax; n++) {
        if (fileDetails[n].sort == 11) {
            fileDetails[n].sort = 1;
            fileDetails[n].fileName = fileName;
            fileDetailsIndex = n;
        }
    }
    setRecentList();
}




QString MainWindow::getWavRecFileName(int n)
{
    QFileInfo fInfo(fileName);
    QString wavName = fInfo.fileName();
    wavName.chop(4);
    wavName.append("_" + Nom[n]->text());
    //QString str = fInfo.absolutePath() + QDir::separator() + recDir + QDir::separator() + wavName;
    return wavName;
}



void MainWindow::setRecentList()
{
    disconnect(ui->comboBoxRecentFiles, SIGNAL(currentIndexChanged(int)), this, SLOT(recentListSelect(int)));
    ui->comboBoxRecentFiles->clear();
    //qDebug() << "add " + fileName;

    for (int index=1; index<=fileDetailsMax; index++) {
    for (int n=0; n<fileDetailsMax; n++) { if ((index == fileDetails[n].sort) && (!fileDetails[n].fileName.isEmpty())) ui->comboBoxRecentFiles->addItem(fileDetails[n].fileName);
    } }
    connect(ui->comboBoxRecentFiles, SIGNAL(currentIndexChanged(int)), this, SLOT(recentListSelect(int)));
}


void MainWindow::playNow()
{
    if (!sndFile) return;
    //if (Paused) { Paused = 0; return; }
    if (Runing) return;
    PaError err;
    const PaDeviceInfo* inputInfo[nbInstru];
    const PaDeviceInfo* outputInfo[nbInstru];

    for (int n=0; n<nbInstru; n++) {
        if (Stream[n] != nullptr) return; }

    err = Pa_Initialize();
    if( err != paNoError ) { qDebug() << QString("Error Pa_Initialize %1").arg(err); Pa_Terminate(); return; }
    /* set the output parameters */
    for (int n=0; n<nbInstru; n++) {
        sf_presetreverb(pa_data[n].rv, sfInfo.samplerate, SF_REVERB_PRESET_DEFAULT);
        pa_data[n].buffer_in = new sf_sample_st[FRAME_SIZE];
        pa_data[n].buffer_out = new sf_sample_st[FRAME_SIZE];
        pa_data[n].frames = Frames;
        pa_data[n].levelLeft = 0;
        pa_data[n].levelRight = 0;
        int r = ReverbSelect[n]->currentIndex() - 1;
        if (r == -1) pa_data[n].reverb = false;
        else {
            sf_presetreverb(pa_data[n].rv, sfInfo.samplerate, (sf_reverb_preset)r);
            pa_data[n].reverb = true; }
        inputParameters[n].device = paNoDevice;
        PaDeviceIndex device = paNoDevice;
#if defined(Q_OS_WIN)
        PaDeviceIndex deviceIn = paNoDevice;
#endif
        if (Ecouteur[n]->currentIndex() != -1) {
            //device = setDevice(deviceID[Ecouteur[n]->currentIndex()]);
#if defined(Q_OS_WIN)
            device = deviceID[Ecouteur[n]->currentIndex()];
            outputParameters[n].device = device;
            deviceIn = deviceMicID[Micro[n]->currentIndex()];
            inputParameters[n].device = deviceIn;
            inputInfo[n] = Pa_GetDeviceInfo(deviceIn);
            outputInfo[n] = Pa_GetDeviceInfo(device);
#endif
#if defined(Q_OS_LINUX)
            device = deviceID[Ecouteur[n]->currentIndex()];
            inputParameters[n].device = device;
            outputParameters[n].device = device;
            inputInfo[n] = Pa_GetDeviceInfo(device);
            outputInfo[n] = Pa_GetDeviceInfo(device);
#endif

            outputParameters[n].channelCount = 2; //pa_data[n].sfInfo.channels; /* use the same number of channels as our sound file */
            outputParameters[n].sampleFormat = paFloat32;
            outputParameters[n].suggestedLatency = double(ui->spinBoxLatency->value())/1000; //outputInfo[n]->defaultLowOutputLatency; //0.1; /* 100 ms */
            outputParameters[n].hostApiSpecificStreamInfo = 0; /* no api specific data */

            inputParameters[n].channelCount = 1;
            inputParameters[n].sampleFormat = paFloat32;
            inputParameters[n].suggestedLatency = double(ui->spinBoxLatency->value())/1000; //inputInfo[n]->defaultLowInputLatency; //0.1; /* 100 ms */
            inputParameters[n].hostApiSpecificStreamInfo = 0; /* no api specific data */
            ui->log->append(outputInfo[n]->name + QString(" Max input channels %1").arg(inputInfo[n]->maxInputChannels) + QString("  I:%1  ").arg(inputInfo[n]->defaultSampleRate) + QString("  O:%1  ").arg(outputInfo[n]->defaultSampleRate));
        }
        else
        {
            inputParameters[n].device = paNoDevice;
            outputParameters[n].device = paNoDevice;
        }
    }

    updateLevel();
    QString error;

    for (int n=0; n<nbInstru; n++) {
        streamOk[n] = false;
        if (inputParameters[n].device != paNoDevice) {
            err = Pa_IsFormatSupported(&inputParameters[n], &outputParameters[n], SAMPLE_RATE);
        if (err == paNoError) {
            if (inputInfo[n]->maxInputChannels == 0) {
                err = Pa_OpenStream(&Stream[n],  /* stream is a 'token' that we need to save for future portaudio calls */
                                      0,
                                      &outputParameters[n],
                                      SAMPLE_RATE,  /* use the same sample rate as the sound file */
                                      FRAME_SIZE,  /* let portaudio choose the buffersize */
                                      paNoFlag,  /* no special modes (clip off, dither off) */
                                      Callback,  /* callback function defined above */
                                      &pa_data[n] ); /* pass in our data structure so the callback knows what's up */
            }
            else
            {
                err = Pa_OpenStream(&Stream[n], &inputParameters[n], &outputParameters[n], SAMPLE_RATE, FRAME_SIZE,paNoFlag, Callback, &pa_data[n] );
            }
            if (err == paNoError) { streamOk[n] = true; if (instru == -1) instru = n; }
            else { ui->log->append("Error open stream"); }
        }
        else
        {
            //ui->log->append(outputInfo[n]->name + QString("  does not support sample rate")); //.arg(pa_data[n].sfInfo.samplerate);
            error.append(Pa_GetErrorText(err));
            error.append(" " + Nom[n]->text() + "\n");
            ui->log->append(Pa_GetErrorText(err));
            //ui->log->append(Pa_GetLastHostErrorInfo()->errorText);
            stop();
        }
        }
    }

    bool atLeastOneRunning = false;
    for (int n=0; n<nbInstru; n++) {
        if (streamOk[n]) {
            err = Pa_SetStreamFinishedCallback( Stream[n], &StreamFinished );
            if( err != paNoError ) { ui->log->append(QString("Error stream " + Nom[n]->text() + " finhed")); }
#if defined(Q_OS_LINUX)
            PaAlsa_EnableRealtimeScheduling(Stream[n], true);
#endif
            err = Pa_StartStream( Stream[n] );
            if( err != paNoError ) { ui->log->append(QString("Error start stream " + Nom[n]->text()));
            error.append(QString("Error start stream " + Nom[n]->text())); }
            else {
                ui->log->append(QString("Stream " + Nom[n]->text() + " started"));
                pa_data[n].runing = true;
                atLeastOneRunning = true;
                vuMeter[n]->setEnabled(true);
            }
        }
    }
    if (atLeastOneRunning) {
    Runing = true;
    Paused = true;
    refresh.start(70); }
    else {
        QMessageBox msgBox;
        msgBox.setText(error);
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
        ONOFF();
    }
}





void MainWindow::stop()
{
    finished = false;
    setFocus();
    ui->buttonPlay->setChecked(false);
    ui->buttonRecord->setChecked(false);
    Paused = false;

    if (!Runing) {
        ui->wavSlider->setValue(0);
        int c = sfInfo.frames / (SAMPLE_RATE / 100);
        int s = c / 100;
        int m = s / 60;
        ui->labelTime->setText(QString("%1:%2:%3").arg(m, 2, 10, QChar('0')).arg(s%60, 2, 10, QChar('0')).arg(c%100, 2, 10, QChar('0')));
        for (int n=0; n<nbInstru; n++) pa_data[n].position = 0;
        ui->buttonRecord->setEnabled(false);
        ui->buttonLoad->setEnabled(true);
        ui->comboBoxRecentFiles->setEnabled(true);
        ui->buttonSave->setEnabled(true);
        ui->radioButton44->setEnabled(true);
        ui->radioButton48->setEnabled(true);
        ui->spinBoxLatency->setEnabled(true);
        ui->spinBoxFrameSize->setEnabled(true);
        return;
    }
    bool rec = false;
    refresh.stop();
    for (int n=0; n<nbInstru; n++) {
        PaError err;
        if (Stream[n]) {
            err = Pa_StopStream( Stream[n]);
            if( err != paNoError ) { qDebug() << "Error stop stream"; return; }

            err = Pa_CloseStream( Stream[n] );
            if( err != paNoError ) { qDebug() << "Error close stream"; return; }

            Stream[n] = nullptr;
            vuMeter[n]->setLeftValue(0);
            vuMeter[n]->setRightValue(0); }
        if (pa_data[n].recorded) { rec = true;
            SaveTrack[n]->setEnabled(true);
            FtpUpload[n]->setEnabled(true); }
        if (pa_data[n].buffer_in) { delete[] pa_data[n].buffer_in; pa_data[n].buffer_in = nullptr; }
        if (pa_data[n].buffer_out) { delete[] pa_data[n].buffer_out; pa_data[n].buffer_out = nullptr; }
        pa_data[n].runing = false;
    }
    ui->buttonRecord->setEnabled(rec);
    ui->buttonLoad->setEnabled(true);
    ui->comboBoxRecentFiles->setEnabled(true);
    ui->buttonSave->setEnabled(true);
    ui->radioButton44->setEnabled(true);
    ui->radioButton48->setEnabled(true);
    ui->spinBoxLatency->setEnabled(true);
    ui->spinBoxFrameSize->setEnabled(true);

    Pa_Terminate();
    Runing = false;
    instru = -1;
}



void MainWindow::updateT()
{
    if (instru != -1) {
    int c = pa_data[instru].position / (SAMPLE_RATE / 100);
    int s = c / 100;
    int m = s / 60;
    if (sliderMove) {
        ui->labelTime->setText(QString("%1:%2:%3").arg(m, 2, 10, QChar('0')).arg(s%60, 2, 10, QChar('0')).arg(c%100, 2, 10, QChar('0')));
        ui->wavSlider->setValue(c); } }
    for (int n=0; n<nbInstru; n++) {
        if (vuMeterEnable) {
            if (pa_data[n].levelLeft > 1) pa_data[n].levelLeft = 1;
            if (pa_data[n].levelRight > 1) pa_data[n].levelRight = 1;
            vuMeter[n]->setLeftValue(pa_data[n].levelLeft);
            vuMeter[n]->setRightValue(pa_data[n].levelRight);
            pa_data[n].levelLeft /= 2;
            pa_data[n].levelRight /= 2; }
        if (firstDevice != -1) {
            qint64 d = qint64(pa_data[n].position - pa_data[firstDevice].position)/48;
            if (delayMean[n] > 5) pa_data[n].position -=5;
            if (delayMean[n] < -5) pa_data[n].position +=5;
            delayMean[n] = ((delayMean[n] << 2) + d) / 5;
            if (pa_data[n].runing) frameIndex[n]->setText(QString("%1 ms").arg(delayMean[n]));
        }
    }
    if (theEnd) {
        ui->buttonPlay->setChecked(false);
        ui->buttonRecord->setChecked(false);
        for (int n=0; n<nbInstru; n++) { frameIndex[n]->setText("..."); pa_data[n].position = 0; }
        ui->labelTime->setText(QString("%1:%2:%3").arg(0, 2, 10, QChar('0')).arg(0, 2, 10, QChar('0')).arg(0, 2, 10, QChar('0')));
        ui->wavSlider->setValue(0);
        theEnd = false;
    }
}



void MainWindow::saveFiles(int track)
{
    int b = 0;
    int e = nbInstru;
    if (track != -1) {
        b = track;
        e = track + 1;
    }
    int fileSaved = 0;
    SF_INFO sfinfo ;
    sfinfo.channels = 2;
    sfinfo.samplerate = SAMPLE_RATE;
    sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
    for (int n=b; n<e; n++) {

        if (pa_data[n].recorded == true) {
        QFileInfo fInfo(fileName);
        QString folderRec = fInfo.absolutePath() + QDir::separator() + recDir;
        QString newfName = fInfo.fileName();
        setWindowTitle(title "  Saving file : " + newfName);
        newfName.chop(4);
        QString T = QDateTime::currentDateTime().date().toString("_dd-MM-yyyy");
        newfName.append("_" + Nom[n]->text() + T + ".wav");
        QString wavFileName = folderRec + QDir::separator() + newfName;
        if (pa_data[n].recorded) {
        if (!QDir().exists(folderRec)) { QDir().mkdir(folderRec); }
        if (QDir().exists(folderRec)) {
        SNDFILE *outfile = sf_open(wavFileName.toLocal8Bit(), SFM_WRITE, &sfinfo);
        if (!outfile) { qDebug() << "Error writing file " + wavFileName; return; }
        sf_write_float(outfile, pa_data[n].wavRecord, pa_data[n].frames * 2) ;
        sf_write_sync(outfile);
        sf_close(outfile);
        ui->log->append(wavFileName + " saved");
        fileSaved ++;
        Nom[n]->setToolTip("file saved " + wavFileName);
        fileLoaded[n] = wavFileName;
        setStyleSheet(Nom[n], loaded);
        setWindowTitle(title "File : " + newfName + " Saved");
        SaveTrack[n]->setEnabled(false);
        pa_data[n].recorded = false;
        pa_data[n].fileLoaded = true;
        }
        else ui->log->append(wavFileName + " could not be saved"); } }
    }
    if (fileSaved == 0) ui->log->append("Nothing to save");
}



void MainWindow::setStyleSheet(QLineEdit *line, int style)
{
    switch (style) {
        case loaded :
        line->setStyleSheet("QLineEdit { background-color:#a6ff4d; \
                              border-width: 2px; \
                              border-radius: 8px; \
                              border-color: beige; \
                              min-width: 10em; \
                              padding: 5px; } ");
        break;
        case notLoaded :
        line->setStyleSheet("QLineEdit { background-color:lightGray; \
                              border-width: 2px; \
                              border-radius: 8px; \
                              border-color: beige; \
                              min-width: 10em; \
                              padding: 5px; } ");
        default :
        break;
    }
}


void MainWindow::setStyleSheet(QPushButton *button, int style)
{
    switch (style) {
        case mixOff :
        button->setStyleSheet("QPushButton { background-color:lightGray; } ");
        break;
        case mixOn :
        button->setStyleSheet("QPushButton { background-color:#a6ff4d; } ");
        default :
        break;
    }
}


void MainWindow::mute()
{
    muteState = !muteState;
    if (muteState)
        ui->buttonHP->setIcon(QIcon(":/images/music_off.png"));
    else
        ui->buttonHP->setIcon(QIcon(":/images/music_on.png"));
}


void MainWindow::updateEcouteur(myCombo* select)
{
    setFocus();
    int index = -1;
    for (int n=0; n<nbInstru; n++) {
        if (Ecouteur[n] != select) {
            if (Ecouteur[n]->currentIndex() == select->currentIndex()) {
                Ecouteur[n]->setCurrentIndex(-1);
                if (!((pa_data[n].fileLoaded == true) && (pa_data[n].mode == Playback))) pa_data[n].mode = trackOff; }
        }
        else index = n;
    }
    if (index == -1) return;
    if (pa_data[index].mode == trackOff) {
        pa_data[index].mode = Listen;
        updateReplay(); }
    // Choose corresponding mic
    QString ecouteur_Name = select->currentText();
    int p = ecouteur_Name.indexOf("(");
    int d = ecouteur_Name.indexOf(":");
    if ((p != -1) && (d != -1)) {
    QString end_of = ecouteur_Name.mid(p, ecouteur_Name.length() - p - 6);
    QString begin_of = ecouteur_Name.mid(0, d);
    QString sr = ecouteur_Name.right(5);
    if (!end_of.isEmpty()) {
        for (int i=0; i<deviceMicName.count(); i++) {
            if (deviceMicName.at(i).contains(end_of) && (deviceMicName.at(i).endsWith(sr)) && (deviceMicName.at(i).startsWith(begin_of))) {
                Micro[index]->setCurrentText(deviceMicName.at(i));
    } } } }
}


void MainWindow::updateMicro(myCombo* select)
{
    setFocus();
    for (int n=0; n<nbInstru; n++) {
        if (Micro[n] != select) {
            if (Micro[n]->currentIndex() == select->currentIndex())
                Micro[n]->setCurrentIndex(-1);
        }
    }
}

void MainWindow::updateReverb(QComboBox* select)
{
    setFocus();
    for (int n=0; n<nbInstru; n++) {
        if (ReverbSelect[n] == select) {
            int r = ReverbSelect[n]->currentIndex() - 1;
            if (r == -1) pa_data[n].reverb = false;
            else {
                sf_presetreverb(pa_data[n].rv, sfInfo.samplerate, (sf_reverb_preset)r);
                pa_data[n].reverb = true; }
        }
    }
}


// Return the date of a file tag
// file name must end with xxx_01-12-2020.wav

QDate MainWindow::getFileDate(QString fileName)
{
    QString d = fileName.right(14).left(10);
    QDate date = QDate::fromString(d, "dd-MM-yyyy");
    return date;
}





void MainWindow::nextDownload()
{
    if (downloadQueue.isEmpty()) return;
    QString url = ui->lineEditDownloadSite->text();
    if (!url.endsWith("/")) url.append("/");
    url.append(downloadQueue.first());
    setWindowTitle(title "  Start download : " + url);
    QUrl wavUrl(url);
    QFileInfo fInfo(fileName);
    // check if Record folder exists
    QString folderRec = fInfo.absolutePath() + QDir::separator() + recDir;
    if (!QDir().exists(folderRec)) { QDir().mkdir(folderRec); }
    if (QDir().exists(folderRec)) {
        QString fName = fInfo.absolutePath() + QDir::separator() + recDir + QDir::separator() + downloadQueue.first();
        downloadFile = new QFile(fName);
        downloadFile->open(QIODevice::WriteOnly);
        webDownload = new QNetworkAccessManager();
        htmlReply = webDownload->get(QNetworkRequest(wavUrl));
        connect(htmlReply,SIGNAL(downloadProgress(qint64,qint64)),this,SLOT(onDownloadProgress(qint64,qint64)));
        connect(webDownload, SIGNAL(finished(QNetworkReply*)),this, SLOT(downloadFinished(QNetworkReply*)));
        connect(htmlReply,SIGNAL(readyRead()),this,SLOT(onReadyRead()));
        connect(htmlReply,SIGNAL(finished()),this,SLOT(onReplyFinished()));}
}


void MainWindow::downloadFinished(QNetworkReply*)
{
    setWindowTitle(title);
    downloadTimer.start(2000);
    webDownload = nullptr;
}

void MainWindow::onReadyRead()
{
    if (downloadFile) downloadFile->write(htmlReply->readAll());
}

void MainWindow::onDownloadProgress(qint64 progress, qint64 total)
{
    int size = total / 1024;
    QString unit = " Ko";
    if (size > 1024) {
        size = size / 1024;
        unit = " Mo";
    }
    qint64 p = 0;
    if (total != 0) p = (progress*100)/total;
    QString txt = QString(" Donwload in progress %1% of %2").arg(p).arg(size);
    setWindowTitle(title + txt + unit);
}



void MainWindow::onReplyFinished()
{
    if (htmlReply->error()) {
        downloadQueue.removeFirst();
        downloadQueueID.removeFirst();
        webDownload->deleteLater();
        ui->log->append(htmlReply->errorString());
    }
    setWindowTitle(title);
    int id = downloadQueueID.first();
    QString fName = downloadFile->fileName();
    if(downloadFile->isOpen())
    {
        downloadFile->close();
        downloadFile->deleteLater();
    }
    // load wav file to memory
    if (downloadFile->exists()) {
    bool converted = false;
    int64_t wavSize = loadWavFile(fName, pa_data[id].wavRecord, converted);
    if (wavSize >= Frames) {
        ui->log->append("file loaded " + fName);
        Nom[id]->setToolTip("file loaded " + fName);
        fileLoaded[id] = fName;
    pa_data[id].fileLoaded = true;
    if (Ecouteur[id]->currentIndex() == -1) pa_data[id].mode = Playback;
    } else Nom[id]->setToolTip(QString("File size %1 doesn't match %2, file not loaded \n").arg(wavSize).arg(Frames) + fName);
    } else Nom[id]->setToolTip("File not found \n" + fName);
    if (pa_data[id].fileLoaded == true) setStyleSheet(Nom[id], loaded); else setStyleSheet(Nom[id], notLoaded);
    downloadQueue.removeFirst();
    downloadQueueID.removeFirst();
    webDownload->deleteLater();
}

void MainWindow::uploadRecWavToFTP(int i)
{
    if (ui->lineEditUploadSite->text().isEmpty())
    {
        QMessageBox msgBox;
        msgBox.setText("Upload site is empty");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
        if (ui->log->isHidden()) logToggle();
        return;
    }
    if (webUpload) {
        QMessageBox msgBox;
        msgBox.setText("Upload in progress\nDo you want to cancel it ?");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        int a = msgBox.exec();
        if (a == QMessageBox::Yes) { if (ftpReply) ftpReply->abort();
        }
        return;
    }
    // Check if file already exists
    // Proceed to upload
    webUpload = new QNetworkAccessManager();
    uploadFile = new QFile(fileLoaded[i]);

    // Next, you need information about the file name
    // The upload path to the server should look like this
    // ftp://example.com/path/to/file/filename.txt
    // That is, we specify the protocol -> ftp
    // Server -> example.com
    // The path where the file will be located -> path/to/file/
    // And the name of the file itself, which we take from QFileInfo -> filename.txt
    QFileInfo fileInfo(*uploadFile);
    QString sUrl = ui->lineEditUploadSite->text();
    if (!sUrl.endsWith("/")) sUrl.append("/");
    sUrl.append(fileInfo.fileName());
    QUrl url(sUrl);
    //url.setUserName("HeadBangers");
    url.setUserName(ui->lineEditFTPLogin->text());
    //url.setPassword("OnJoueTousEnsemble");
    url.setPassword(pwd->text());
    url.setPort(21);
    if (uploadFile->open(QIODevice::ReadOnly))
    {
        // Start upload
        ui->log->append("Start FTP upload for : " + fileLoaded[i]);
        ftpReply = webUpload->put(QNetworkRequest(url), uploadFile);
        // And connect to the progress upload signal
        connect(webUpload, SIGNAL(finished(QNetworkReply*)),this, SLOT(ftpRequestFinished(QNetworkReply*)));
        connect(ftpReply, SIGNAL(uploadProgress(qint64, qint64)), this, SLOT(ftpUploadProgress(qint64, qint64)));
        connect(ftpReply,SIGNAL(finished()),this,SLOT(ftpUploadDone()));
        connect(ftpReply, SIGNAL(errorOccurred(QNetworkReply::NetworkError)), SLOT(ftpUploadError(QNetworkReply::NetworkError)));
    }
    // https://www.w3schools.com/php/php_file_upload.asp
}


//void errorOccurred(QNetworkReply::NetworkError);

void MainWindow::ftpUploadError(QNetworkReply::NetworkError error)
{
    ui->log->append(ftpReply->errorString());
    ui->log->append(QString("Error : %1").arg(error));
}

void MainWindow::ftpUploadDone()
{
    //qDebug() << ftpReply->errorString();
}


void MainWindow::ftpRequestFinished(QNetworkReply* reply)
{
    if(reply->error()) { ui->log->append(reply->errorString());
    setWindowTitle(title "  FTP upload " + reply->errorString()); }
    else {
    ui->log->append("FTP upload finished");
    setWindowTitle(title "  FTP upload finished"); }
    uploadFile->close();
    uploadFile->deleteLater();
    reply->deleteLater();
    webUpload->deleteLater();
    webUpload = nullptr;
    ftpReply = nullptr;
}



void MainWindow::ftpUploadProgress(qint64 progress, qint64 total)
{
    // Display the progress of the upload
    int size = total / 1024;
    QString unit = " Ko";
    if (size > 1024) {
        size = size / 1024;
        unit = " Mo";
    }
    qint64 p = 0;
    if (total != 0) p = (progress*100)/total;
    QString txt = QString(" FTP upload in progress %1% of %2").arg(p).arg(size);
    setWindowTitle(title + txt + unit);
}



void MainWindow::mixAsChanged(QSpinBox *mix)
{
    for (int n=0; n<nbInstru; n++) {
        for (int i=0; i<nbInstru; i++) {
        if (MixSend[n][i] == mix) {
            int delta = MixSend[n][i]->value() - mixValue[n][i] ;
            mixValue[n][i] = MixSend[n][i]->value();
            if (mixCheckAllChannels[n]->isChecked())
            {
                for (int x=0; x<nbInstru; x++) {
                    if ((x != n) && (x != pa_data[i].myIndex)) {
                        disconnect(MixSend[x][i], 0, 0, 0);
                        if (QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier)) {
                            MixSend[x][i]->setValue(mix->value());
                        }
                        else {
                            if (delta > 0) MixSend[x][i]->setValue(MixSend[x][i]->value() + 1);
                            if (delta < 0) MixSend[x][i]->setValue(MixSend[x][i]->value() - 1);
                        }
                        connect(MixSend[x][i], SIGNAL(valueChanged(int)), this, SLOT(mixChanged(int)));
                        connect(MixSend[x][i], QOverload<int>::of(&QSpinBox::valueChanged), [=] { mixAsChanged( MixSend[x][i] );  } );
                    }
                }
            }
                if (mixCheckAll[n]->isChecked())
            {
                for (int x=0; x<nbInstru; x++) {
                    if (x != n) {
                        disconnect(MixSend[n][x], 0, 0, 0);
                        if (QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier)) {
                            MixSend[n][x]->setValue(mix->value());
                        }
                        else {
                            if (delta > 0) MixSend[n][x]->setValue(MixSend[n][x]->value() + 1);
                            if (delta < 0) MixSend[n][x]->setValue(MixSend[n][x]->value() - 1);
                        }
                        connect(MixSend[n][x], SIGNAL(valueChanged(int)), this, SLOT(mixChanged(int)));
                        connect(MixSend[n][x], QOverload<int>::of(&QSpinBox::valueChanged), [=] { mixAsChanged( MixSend[n][x] );  } );
                    }
                }
            }
            return;
        } }
    }
}


void MainWindow::saveThisTrack(QPushButton* button)
{
    for (int n=0; n<nbInstru; n++) {
        if (SaveTrack[n] == button) {
            saveFiles(n);
            return;
        }
    }
}


void MainWindow::uploadThisTrack(QPushButton* button)
{
    for (int n=0; n<nbInstru; n++) {
        if (FtpUpload[n] == button) {
            saveFiles(n);
            uploadRecWavToFTP(n);
            return;
        }
    }
}



void MainWindow::showMix(QPushButton* button)
{
    if (QGuiApplication::keyboardModifiers().testFlag(Qt::ControlModifier)) {
        QString name = searchDevice();
        if (!name.isEmpty()) {
            for (int n=0; n<nbInstru; n++) {
                if (Mix[n] == button) {
                    Ecouteur[n]->setCurrentText(name);
                    if (pa_data[n].mode == trackOff) pa_data[n].mode = Listen;
                    updateReplay();
                }
            }
        }
        return;
    }

    if (QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier)) {
        for (int n=0; n<nbInstru; n++) {
            if (Mix[n] == button) {
                if (lastMixOpened == QPoint(0, 0)) lastMixOpened = QPoint(300, 100);
                if (!mixAlreadyShowed[n]) {
                    QDesktopWidget *desktopWidget = QApplication::desktop();
                    QRect screenRect = desktopWidget->screenGeometry(mixSetup[n]);
                    if (screenRect.contains(lastMixOpened)) {
                        mixSetup[n]->move(lastMixOpened);
                        lastMixOpened.setX(lastMixOpened.x() + 60);
                        lastMixOpened.setY(lastMixOpened.y() + 60); }
                    else {
                        lastMixOpened = QPoint(330, 130);
                        mixSetup[n]->move(lastMixOpened);  }
                    }
                mixSetup[n]->show();
                mixAlreadyShowed[n] = true;
                return; } } }
    for (int n=0; n<nbInstru; n++) {
        if (Mix[n] == button) {
            pa_data[n].mixEnabled = !pa_data[n].mixEnabled;
            if (pa_data[n].mixEnabled) setStyleSheet(Mix[n], mixOn); else setStyleSheet(Mix[n], mixOff);
            if (pa_data[n].mixEnabled) {
            mixEnabled = true;
            setStyleSheet(ui->buttonMix, mixOn);
            ui->buttonMix->setToolTip("Mixer ON"); }
        }
    }
}


void MainWindow::getWebFileList()
{
    if (ui->lineEditDownloadSite->text().isEmpty())
    {
        QMessageBox msgBox;
        msgBox.setText("Download site is empty");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
        if (ui->log->isHidden()) logToggle();
        return;
    }
    QUrl wavUrl(ui->lineEditDownloadSite->text());
    htmlFileList.clear();
    webDownload = new QNetworkAccessManager();
    webDownload->setRedirectPolicy(QNetworkRequest::SameOriginRedirectPolicy);
    htmlReply = webDownload->get(QNetworkRequest(wavUrl));
    connect(webDownload, SIGNAL(finished(QNetworkReply*)),this, SLOT(listFinished(QNetworkReply*)));
    connect(htmlReply,SIGNAL(readyRead()),this,SLOT(onReadyReadList()));
    connect(htmlReply,SIGNAL(finished()),this,SLOT(listReplyFinished()));
}

void MainWindow::listFinished(QNetworkReply*)
{
    setWindowTitle(title);
    if (htmlFileList.isEmpty()) {
        ui->log->append("No connection to web site");
        return;
    }
    QString str = ui->log->toPlainText();
    ui->log->clear();
    ui->log->insertHtml(htmlFileList);
    QString html = ui->log->toPlainText();
    ui->log->clear();
    ui->log->append(str);
    ui->log->append(html);
    // htmlTxt contains all the lines of the html page
    QStringList htmlTxt = html.split("\n");
    // filter only line with the on going wav file opened
    QFileInfo fInfo(fileName);
    QString fileOpened = fInfo.fileName();
    if (fileOpened.endsWith(".wav")) fileOpened.chop(4);
    QStringList fileList;
    QList <QDate> dateList;
    foreach (QString line, htmlTxt) {
        if (line.startsWith(fileOpened)) {
            QDate date = getFileDate(line);
            if (date.isValid())
            {
                fileList.append(line);
                dateList.append(date);
            } }
    }
    // now fileList contains all the lines starting with the name of the ongoing wav file opened
    // and a valid date tag at the end stored in dateList
    // Loop for each instrument and get the newest file
    bool found = false;
    for (int n=0; n<nbInstru; n++)
    {
        QString wavFileName = getWavRecFileName(n);
        QDate date;
        QString newestFile;
        // get only the newest file among the web list
        for (int i=0; i<fileList.count(); i++)
        {
            if (fileList.at(i).contains(wavFileName)) {
                if (!date.isValid()) {
                    date = dateList.at(i);
                    newestFile = fileList.at(i); }
                else {
                    if (date.daysTo(dateList.at(i)) > 0) {
                        newestFile = fileList.at(i);
                        date = dateList.at(i); } }
            }
        }
        // compare now file loaded with newest file date
        if (!newestFile.isEmpty()) {
            if (fileLoaded[n].isEmpty()) {
                // No file loaded, ask to load newest file
                if (!downloadQueue.contains(newestFile)) {
                found = true;
                QMessageBox msgBox;
                msgBox.setText("A file for " + Nom[n]->text() + " is available\nDo you want to download it ?\n" + newestFile);
                msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
                int a = msgBox.exec();
                if (a == QMessageBox::Yes) {
                    downloadQueue.append(newestFile);
                    downloadQueueID.append(n);
                    ui->log->append(wavFileName + " is added to download queue");
            } } else ui->log->append(wavFileName + " already in the download queue");
            }
            else {
            QDate actualFileDate = getFileDate(fileLoaded[n]);
            if (actualFileDate.daysTo(date) > 0)
            {
                if (!downloadQueue.contains(newestFile)) {
                // Newest file from html list is newer, ask to download it
                QMessageBox msgBox;
                msgBox.setText("Do you want to donwlolad file\n" + newestFile);
                msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
                found = true;
                int a = msgBox.exec();
                if (a == QMessageBox::Yes) {
                    downloadQueue.append(newestFile);
                    downloadQueueID.append(n);
                    ui->log->append(wavFileName + " is added to download queue");
                } } else ui->log->append(wavFileName + " already in the download queue");
            } else {
                ui->log->append("No newer file found for " + wavFileName);
            } }
        }
        else {
            ui->log->append("No newer file found for " + wavFileName);
        }
    }
    if (!found) {
        QMessageBox msgBox;
        msgBox.setText("No newer file found");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
    }
    if (!downloadQueue.isEmpty()) downloadTimer.start(1000);
    webDownload = nullptr;
}

void MainWindow::onReadyReadList()
{

    htmlFileList.append(htmlReply->readAll());
}

void MainWindow::listReplyFinished()
{
    //setWindowTitle(title);
    if (htmlReply->error()) {
        ui->log->append(htmlReply->errorString());
        htmlFileList.clear();
    }
}


void MainWindow::mixChanged(int)
{
    for (int n=0; n<nbInstru; n++) {
        for (int i=0; i<nbInstru; i++) {
            int mix = MixSend[n][i]->value();
            if (mixValue[n][i] == -99) mixValue[n][i] = mix;
            if (mix == -41) pa_data[n].mixGain[i] = 0;
            else pa_data[n].mixGain[i] = double(pow(10.0, double(mix /20.0)));
        }
    }
}


void MainWindow::nomChanged(QLineEdit* line)
{
    for (int n=0; n<nbInstru; n++) {
        if (Nom[n] == line) {
            mixSetup[n]->setWindowTitle(Nom[n]->text());
            }
        for (int i=0; i<nbInstru; i++) {
            MixSend[n][i]->setSpecialValueText(Nom[i]->text() +  " : OFF");
            MixSend[n][i]->setPrefix((Nom[i]->text() +  " : "));
        }
    }
}



void MainWindow::updateReplay()
{
    for (int n=0; n<nbInstru; n++) {
        switch (pa_data[n].mode) {
            case Playback : ModeButton[n]->setIcon(QIcon(":/images/replay"));
                            ModeButton[n]->setToolTip("Play back");
                            L_In[n]->setStyleSheet("QSpinBox { background-color:#80bfff }");
                            L_In[n]->setValue(pa_data[n].PlayVol);
                            break;
            case Record :   ModeButton[n]->setIcon(QIcon(":/images/microrouge"));
                            ModeButton[n]->setToolTip("Record");
                            L_In[n]->setStyleSheet("QSpinBox { background-color:#ff8080 }");
                            L_In[n]->setValue(pa_data[n].RecVol);
                            break;
            case Listen :   ModeButton[n]->setIcon(QIcon(":/images/microvert"));
                            ModeButton[n]->setToolTip("Listen");
                            L_In[n]->setStyleSheet("QSpinBox { background-color:#a6ff4d }");
                            L_In[n]->setValue(pa_data[n].ListenVol);
                            break;
            case trackOff :
            default :       ModeButton[n]->setIcon(QIcon(":/images/microoff"));
                            ModeButton[n]->setToolTip("Off");
                            L_In[n]->setStyleSheet("QSpinBox { background-color: lightgrey }");
                            break;
        }
    }
}


void MainWindow::updateReplay(QPushButton* button)
{
    bool rec = false;
    for (int n=0; n<nbInstru; n++) {
        if ((ModeButton[n] == button) || (button == nullptr)) {
        if (QGuiApplication::keyboardModifiers().testFlag(Qt::ControlModifier)) {
            // When you holg Control key, all device get the same mode
           for (int i=0; i<nbInstru; i++) {
                pa_data[i].mode = pa_data[n].mode;
                    switch (pa_data[n].mode) {
                        case Playback : ModeButton[i]->setIcon(QIcon(":/images/replay"));
                                        ModeButton[i]->setToolTip("Play back");
                                        L_In[i]->setStyleSheet("QSpinBox { background-color:#80bfff }");
                                        L_In[i]->setValue(pa_data[i].G_In);
                                        break;
                        case Record :   ModeButton[i]->setIcon(QIcon(":/images/microrouge"));
                                        ModeButton[i]->setToolTip("Record");
                                        L_In[i]->setStyleSheet("QSpinBox { background-color:#ff8080 }");
                                        L_In[i]->setValue(pa_data[i].G_Rec);
                                        break;
                        case Listen :   ModeButton[i]->setIcon(QIcon(":/images/microvert"));
                                        ModeButton[i]->setToolTip("Listen");
                                        L_In[i]->setStyleSheet("QSpinBox { background-color:#a6ff4d }");
                                        L_In[i]->setValue(pa_data[i].G_Play);
                                        break;
                        case trackOff :
                        default :       ModeButton[i]->setIcon(QIcon(":/images/microoff"));
                                        ModeButton[i]->setToolTip("Off");
                                        L_In[i]->setStyleSheet("QSpinBox { background-color: lightgrey }");
                                        //L_In[i]->setValue(-41);
                                        break;
                    }
                }
            }
            else {
                if (Ecouteur[n]->currentIndex() == -1) {
                    if ((pa_data[n].fileLoaded == false) && (pa_data[n].recorded == false)) pa_data[n].mode = trackOff;
                    else if (pa_data[n].mode != trackOff) pa_data[n].mode = trackOff; else pa_data[n].mode = Playback;
                } else pa_data[n].mode ++;
                if (pa_data[n].mode > trackOff) pa_data[n].mode = Listen;
                updateReplay();
            }
        }
        if (pa_data[n].mode == Record) rec = true;
    }
    if (turnedON) {
    ui->buttonRecord->setEnabled(rec); }
    if (!rec && !Paused && Recording) {
        Recording = false;
        ui->buttonPlay->setChecked(true);
        ui->buttonRecord->setChecked(false); }
    updateLevel();
}



void MainWindow::updateReverb()
{
    for (int n=0; n<nbInstru; n++) {
        pa_data[n].RevLev = Reverb_Level[n]->value();
        pa_data[n].G_RevLev = double(pow(10.0, double(pa_data[n].RevLev /20.0)));
    }
}


void MainWindow::updateLevel()
{
    for (int n=0; n<nbInstru; n++) {
        switch (pa_data[n].mode) {
            case Playback :
                if (L_In[n]->value() > 0) L_In[n]->setPrefix("Rep : +"); else L_In[n]->setPrefix("Rep : ");
                pa_data[n].PlayVol = L_In[n]->value();
                break;
            case Record :
                if (L_In[n]->value() > 0) L_In[n]->setPrefix("Rec : +"); else L_In[n]->setPrefix("Rec : ");
                pa_data[n].RecVol = L_In[n]->value();
            break;
            case Listen :
                if (L_In[n]->value() > 0) L_In[n]->setPrefix("Mic : +"); else L_In[n]->setPrefix("Mic : ");
                pa_data[n].ListenVol = L_In[n]->value();
            break;
        default :
        case trackOff :
            L_In[n]->setValue(-41);
            break;
        }
        if (L_Out[n]->value() > 0) L_Out[n]->setPrefix("File : +");
        else L_Out[n]->setPrefix("File : ");
        pa_data[n].G_In = double(pow(10.0, double(pa_data[n].ListenVol /20.0)));
        pa_data[n].G_Play = double(pow(10.0, double(pa_data[n].PlayVol/20.0)));
        pa_data[n].G_Rec = double(pow(10.0, double(pa_data[n].RecVol/20.0)));
        pa_data[n].G_Wav = double(pow(10.0, double(L_Out[n]->value()/20.0))); }
}



void MainWindow::beginChanged(QTime t)
{
    setLoopBegin(t.msecsSinceStartOfDay() * (SAMPLE_RATE / 1000));
}


void MainWindow::endChanged(QTime t)
{
    setLoopEnd(t.msecsSinceStartOfDay() * (SAMPLE_RATE / 1000));
}


QTime MainWindow::loopIdx2Time(quint64 idx)
{
    quint64 z = idx / (SAMPLE_RATE / 1000);
    quint64 c = idx / (SAMPLE_RATE / 100);
    quint64 s = c / 100;
    quint64
            m = s / 60;
    return QTime(0, m, s%60, z%1000);
}



void MainWindow::setLoopBegin (quint64 v)
{
    //qDebug() << QString("setLooBegin %1").arg(v);
    if (!Runing) return;
    if (v > quint64(Frames)) {
        v = quint64(Frames) - 1;
        ui->timeBegin->setStyleSheet("QTimeEdit { background-color: red; }");
    }
    else {
        ui->timeBegin->setStyleSheet("QTimeEdit { background-color: lightgrey; }");
    }
    loop_begin = v;
    disconnect(ui->timeBegin, SIGNAL(timeChanged(QTime)), this, SLOT(beginChanged(QTime)));
    ui->timeBegin->setTime(loopIdx2Time(loop_begin));
    connect(ui->timeBegin, SIGNAL(timeChanged(QTime)), this, SLOT(beginChanged(QTime)));
    if (fileDetailsIndex != -1) { fileDetails[fileDetailsIndex].loop_begin = loopIdx2Time(loop_begin); }
    if (loop_end < loop_begin) {
        loop = false;
        loopRecord = false;
        ui->checkBoxLoop->setEnabled(false);
        ui->checkBoxLoopRecord->setEnabled(false);
    }
    else {
        ui->checkBoxLoop->setEnabled(true);
        if (ui->checkBoxLoop->isChecked()) loop = true;
        ui->checkBoxLoopRecord->setEnabled(true);
        if (ui->checkBoxLoopRecord->isChecked()) loopRecord = true;
    }
}


void MainWindow::setLoopEnd (quint64 v)
{
    //qDebug() << QString("setLoopEnd %1").arg(v);
    if (!Runing) return;
    if (v > loop_begin) {
    if (v > quint64(Frames)) {
        v = quint64(Frames);
        ui->timeEnd->setStyleSheet("QTimeEdit { background-color: red; }");
    }
    else {
        ui->timeEnd->setStyleSheet("QTimeEdit { background-color: lightgrey; }");
    }
    loop_end = v;
    disconnect(ui->timeEnd, SIGNAL(timeChanged(QTime)), this, SLOT(endChanged(QTime)));
    ui->timeEnd->setTime(loopIdx2Time(loop_end));
    connect(ui->timeEnd, SIGNAL(timeChanged(QTime)), this, SLOT(endChanged(QTime)));
    }
    if (fileDetailsIndex != -1) { fileDetails[fileDetailsIndex].loop_end = loopIdx2Time(loop_end); }
    if (loop_end < loop_begin) {
        loop = false;
        loopRecord = false;
        ui->checkBoxLoop->setEnabled(false);
        ui->checkBoxLoopRecord->setEnabled(false);
    }
    else {
        ui->checkBoxLoop->setEnabled(true);
        if (ui->checkBoxLoop->isChecked()) loop = true;
        ui->checkBoxLoopRecord->setEnabled(true);
        if (ui->checkBoxLoopRecord->isChecked()) loopRecord = true;
    }
}


void MainWindow::keyPressEvent(QKeyEvent *e)
{
    if(e->key() == Qt::Key_Space) { play(); }
    if(e->key() == Qt::Key_Left) {
        if (Runing) { for (int n=0; n<nbInstru; n++) { if (pa_data[n].position > 100000)  pa_data[n].position -= 100000; } } }
    if(e->key() == Qt::Key_Right) {
        if (Runing) { for (int n=0; n<nbInstru; n++) { if (pa_data[n].position < (pa_data[n].frames-100000))  pa_data[n].position += 100000; } } }
    if(e->key() == Qt::Key_F1) { setLoopBegin(pa_data[instru].position); }
    if(e->key() == Qt::Key_F2) { setLoopEnd(pa_data[instru].position); }
    if(e->key() == Qt::Key_R) {
        if (Runing) {
                if (loop == false) {
                    for (int n=0; n<nbInstru; n++) { pa_data[n].position = 0; } }
                else {
                    for (int n=0; n<nbInstru; n++) { pa_data[n].position = loop_begin; } } }
                }
    if(e->key() == Qt::Key_L) {
        if (ui->checkBoxLoop->isChecked()) { ui->checkBoxLoop->setCheckState(Qt::Unchecked); loop = false;} else { ui->checkBoxLoop->setCheckState(Qt::Checked); loop = true;}
    }
    if(e->key() == Qt::Key_F12) {
        logToggle();
    }
}


void MainWindow::closeEvent(QCloseEvent *)
{
        QCoreApplication::exit(0);
}

