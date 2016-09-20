/********************************************************************
 * comedirecord.cpp 
 * License: GNU, GPL
 * (c) 2004-2016, Bernd Porr
 * No Warranty
 ********************************************************************/

#include <sys/ioctl.h>
#include <math.h>

#include <QTimer>
#include <QPainter>
#include <QApplication>
#include <QButtonGroup>
#include <QGroupBox>
#include <QFileDialog>
#include <QSizePolicy>
#include <QSettings>
#include <QScrollArea>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <qtextedit.h>
#include <qfont.h>


#include <iostream>
#include <fstream>

#include "comediscope.h"
#include "comedirecord.h"

// version number
#define VERSION "1.24"

// config constants
#define SETTINGS_GLOBAL "global"
#define SETTINGS_CHANNELS "channelconfig"
#define CHSETTING_FORMAT "ch_mapping_dev%09d_ch%09d"
#define USBDUX_STRING "USB-DUX"
#define PROGRAM_NAME "comedirecord"

class QCommentTextEdit : public QTextEdit {
protected:
	void keyPressEvent(QKeyEvent *event) {
		if ((event->key() != Qt::Key_Return)&&
		    (event->key() != Qt::Key_Enter)&&
		    (event->key() != Qt::Key_Tab)	)	{
			QTextEdit::keyPressEvent(event);
		}
	}
};


ComediRecord::ComediRecord( QWidget *parent, 
			    int ignoreSettings,
			    int nchannels,
			    float notchF,
			    int port,
			    int num_of_devices,
			    int first_dev_no,
			    int requrested_sampling_rate,
			    const char* defaultTextStringForMissingExtData,
			    const char* filename,
			    int csv,
			    int fftdev, 
			    int fftch,
			    int fftmaxf,
			    float lpFreq,
			    float hpFreq
	)
    : QWidget( parent ) {

        comediScope=new ComediScope(this,
				    nchannels,
				    notchF,
				    port,
				    num_of_devices,
				    first_dev_no,
				    requrested_sampling_rate,
				    defaultTextStringForMissingExtData,
				    fftdev, 
				    fftch,
				    fftmaxf
		);

	int n_devs = comediScope->getNcomediDevices();
	int channels = comediScope->getNchannels();

	if ((fftdev>=0)&&(fftdev<n_devs)&&(fftch>=0)&&(fftch<channels)) {
		fftscope = new FFTScope( this );
		fftscope->show();
	} else {
		fftscope = NULL;
	}

	tb_us = 1000000 / comediScope->getActualSamplingRate();

	// fonts
	QFont voltageFont("Courier",10);
	QFontMetrics voltageMetrics(voltageFont);

	// this the main layout which contains two sub-windows:
	// the control window and the oscilloscope window
	QHBoxLayout *mainLayout = new QHBoxLayout;
	mainLayout->setSpacing(0);
	setLayout(mainLayout);

	// the corresponding box which contains all the controls
	QGroupBox *controlBox = new QGroupBox ();
	controlBox->setStyleSheet("padding:0px;margin:0px;border:0px;");
	controlBox->setSizePolicy ( QSizePolicy(QSizePolicy::Fixed,
						QSizePolicy::Minimum ) );

	// now we create another layout which contains all the controls
	QVBoxLayout * controlLayout = new QVBoxLayout(controlBox);
	// this is the vertical layout for all the controls
	QVBoxLayout *scopeLayout = new QVBoxLayout(0);
	// the corresponding box which contains all the controls
	QGroupBox *scopeGroup = new QGroupBox ();

	// we create a scroll area
	QScrollArea *allChScrollArea = new QScrollArea();
	// this is the layout containing the scrollArea
	QVBoxLayout *allChScrollLayout = new QVBoxLayout();
	// to this layout we add just one widget which is the scroll area
	allChScrollLayout->addWidget(allChScrollArea);


	QGridLayout *allChLayout = new QGridLayout;
	QGroupBox *allChGroup = new QGroupBox;
	allChLayout->setSpacing(0);
	allChGroup->setStyleSheet("padding:0px;margin:0px;border:0px;");
	allChGroup->setLayout(allChLayout);
	allChGroup->setSizePolicy ( QSizePolicy(QSizePolicy::Fixed,
						QSizePolicy::Expanding ) );

	int row = 1;

	channelLabel=new QLabel**[n_devs];
	channel=new Channel**[n_devs];
	voltageTextEdit=new QTextEdit**[n_devs];
	channelgrp=new QGroupBox**[n_devs];
	hbox=new QHBoxLayout**[n_devs];
	gain=new Gain**[n_devs];
	dcSub=new DCSub**[n_devs];
	hp=new Hp**[n_devs];
	lp=new Lp**[n_devs];
	subDClabel=new QLabel**[n_devs];
	hpLabel=new QLabel**[n_devs];
	lpLabel=new QLabel**[n_devs];
	
	// to the get the stuff a bit closer together
	char styleSheet[] = "padding:0px;margin:0px;border:0px;";

	QSettings settings(QSettings::IniFormat, 
			   QSettings::UserScope,
			   USBDUX_STRING,
			   PROGRAM_NAME);

	settings.beginGroup(SETTINGS_CHANNELS);

	int nch_enabled = 0;
	for(int n=0;n<n_devs;n++) {
		channelLabel[n]=new QLabel*[channels];
		channel[n]=new Channel*[channels];
		voltageTextEdit[n]=new QTextEdit*[channels];
		channelgrp[n]=new QGroupBox*[channels];
		hbox[n]=new QHBoxLayout*[channels];
		gain[n]=new Gain*[channels];
		dcSub[n]=new DCSub*[channels];
		subDClabel[n]=new QLabel*[channels];
		hp[n]=new Hp*[channels];
		hpLabel[n]= new QLabel*[channels];
		lp[n]=new Lp*[channels];
		lpLabel[n]= new QLabel*[channels];
		for(int i=0;i<channels;i++) {
			// create the group for a channel
			char tmp[10];
			channelgrp[n][i] = new QGroupBox();
			channelgrp[n][i]->setStyleSheet(styleSheet);
			// the corresponding layout
			hbox[n][i] = new QHBoxLayout();
			channelgrp[n][i]->setLayout(hbox[n][i]);
			sprintf(tmp,"%02d:",i);
			channelLabel[n][i] = new QLabel(tmp);
			channelLabel[n][i]->setStyleSheet(styleSheet);
			channelLabel[n][i]->setFont(voltageFont);
			hbox[n][i]->addWidget(channelLabel[n][i]);
			hbox[n][i]->setSpacing(1);
			channel[n][i] = new Channel(channels);
			char tmpCh[128];
			sprintf(tmpCh,CHSETTING_FORMAT,n,i);
			if (ignoreSettings) {
				channel[n][i] -> setChannel( i );
			} else {
				channel[n][i] -> setChannel(
					settings.value(tmpCh,i).toInt() );
			}
			if ( channel[n][i] -> isActive() )
				nch_enabled++;
			channel[n][i]->setStyleSheet(styleSheet);
			hbox[n][i]->addWidget(channel[n][i]);
			voltageTextEdit[n][i]=new QTextEdit(channelgrp[n][i]);
			voltageTextEdit[n][i]->setStyleSheet(styleSheet);
			hbox[n][i]->addWidget(voltageTextEdit[n][i]);
			voltageTextEdit[n][i]->setFont(voltageFont);
			char tmpVolt[128];
			sprintf(tmpVolt,
				" "VOLT_FORMAT_STRING" ",
				comediScope->crange[n]->max);
			voltageTextEdit[n][i]->setMaximumSize
				(voltageMetrics.width(tmpVolt),
				 (int)(voltageMetrics.height()*1.1));
			voltageTextEdit[n][i]->
				setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff ); 
			voltageTextEdit[n][i]->setReadOnly(true);
			voltageTextEdit[n][i]->setFont(voltageFont);
			// voltageTextEdit[i]->setLineWidth(1);

			subDClabel[n][i] = new QLabel(" -DC");
			subDClabel[n][i]->setStyleSheet(styleSheet);
			subDClabel[n][i]->setFont(voltageFont);
			hbox[n][i]->addWidget(subDClabel[n][i]);

			dcSub[n][i] = new DCSub((float)INERTIA_FOR_DC_DETECTION);
			dcSub[n][i]->setStyleSheet(styleSheet);
			hbox[n][i]->addWidget(dcSub[n][i]);

			hpLabel[n][i] = new QLabel(" HP");
			hpLabel[n][i]->setStyleSheet(styleSheet);
			hpLabel[n][i]->setFont(voltageFont);
			hbox[n][i]->addWidget(hpLabel[n][i]);

			hp[n][i] = new Hp(comediScope->getActualSamplingRate(),
					  hpFreq);
			hp[n][i] ->setStyleSheet(styleSheet);
			hbox[n][i]->addWidget(hp[n][i]);

			lpLabel[n][i] = new QLabel(" LP");
			lpLabel[n][i]->setStyleSheet(styleSheet);
			lpLabel[n][i]->setFont(voltageFont);
			hbox[n][i]->addWidget(lpLabel[n][i]);

			lp[n][i] = new Lp(comediScope->getActualSamplingRate(),
					  lpFreq);
			lp[n][i] ->setStyleSheet(styleSheet);
			hbox[n][i]->addWidget(lp[n][i]);

			gain[n][i] = new Gain();
			gain[n][i]->setStyleSheet(styleSheet);
			hbox[n][i]->addWidget(gain[n][i]);

			allChLayout->addWidget(channelgrp[n][i],row,1);
			row++;
		}
	}

	settings.endGroup();

	controlLayout->addWidget(allChScrollArea);
	allChScrollArea->setWidget(allChGroup);

	// at least one should be active not to make the user nervous.
	if (nch_enabled==0)
		channel[0][0]->setChannel( 0 );

	// now we create another layout which contains all the remaining controls
	QVBoxLayout * restLayout = new QVBoxLayout;
	// the corresponding box which contains all the controls
	QGroupBox *restGroup = new QGroupBox;

	// notch filter
	// create a group for the notch filter
	QGroupBox* notchGroupBox = new QGroupBox();
	notchGroupBox->setFlat(true);
	notchGroupBox->setStyleSheet(styleSheet);
	QHBoxLayout *notchLayout = new QHBoxLayout();
	char tmp[128];
	sprintf(tmp,"%2.0fHz notch",notchF);
	filterCheckbox=new QCheckBox( tmp );
	filterCheckbox->setChecked( false );
	notchLayout->addWidget(filterCheckbox);
	commentTextEdit=new QCommentTextEdit();
	QFont commentFont("Courier",10);
	QFontMetrics commentMetrics(commentFont);
	commentTextEdit->setMaximumHeight ( commentMetrics.height() );
	commentTextEdit->setMaximumWidth ( 10*commentMetrics.width('X') );
	commentTextEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	commentTextEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	commentTextEdit->setFont(commentFont);
	QLabel *l=new QLabel("Comment:");
	notchLayout->addWidget(l);
	notchLayout->addWidget(commentTextEdit);
       	notchGroupBox->setLayout(notchLayout);
	restLayout->addWidget(notchGroupBox);


	// group for the record stuff
	QGroupBox* recGroupBox = new QGroupBox();
	QHBoxLayout *recLayout = new QHBoxLayout();

	QLabel* recLabel = new QLabel("Write to file:");
	recLayout->addWidget(recLabel);

	filePushButton = new QPushButton( "&filename" );
	filePushButton->setSizePolicy ( QSizePolicy(QSizePolicy::Fixed,
						    QSizePolicy::Fixed ));
	filePushButton->setStyleSheet(
		"background-color: white;border-style:outset;border-width: 2px;border-color: black;font: bold 14px; padding: 0px;");
	connect(filePushButton, SIGNAL( clicked() ),
		this, SLOT( enterFileName() ) );
	recLayout->addWidget(filePushButton);

	rawCheckbox=new QCheckBox("raw data" );
	rawCheckbox->setChecked( false );
	recLayout->addWidget(rawCheckbox);

	recPushButton = new QCheckBox( "&REC" );
	recPushButton->connect(recPushButton, SIGNAL( stateChanged(int) ),
			       this, SLOT( recstartstop(int) ) );
	recPushButton->setEnabled( false );
	recLayout->addWidget(recPushButton);

	recGroupBox->setLayout(recLayout);
	restLayout->addWidget(recGroupBox);


	// group for the time base
	QGroupBox *tbgrp = new QGroupBox();
	QHBoxLayout *tbLayout = new QHBoxLayout;
	QFont tbFont("Courier",12);
	tbFont.setBold(true);
	QFontMetrics tbMetrics(tbFont);

	QLabel* tbLabel = new QLabel("Timebase:");
	tbLayout->addWidget(tbLabel);

	tbIncPushButton = new QPushButton( "+" );

	char tbStyle[]="background-color: white;border-style:outset;border-width: 2px;border-color: black;font: bold 20px;padding: 4px;";
	tbIncPushButton->setStyleSheet(tbStyle);
	tbIncPushButton->setMaximumSize ( tbMetrics.width(" + ") ,  
					  tbMetrics.height() );
	tbIncPushButton->setFont(tbFont);
	tbgrp->connect(tbIncPushButton, SIGNAL( clicked() ),
		this, SLOT( incTbEvent() ) );
	tbLayout->addWidget(tbIncPushButton);

	tbDecPushButton = new QPushButton( "-" );
	tbDecPushButton->setStyleSheet(tbStyle);
	tbDecPushButton->setMaximumSize ( tbMetrics.width(" + ") ,  
					  tbMetrics.height() );
	tbDecPushButton->setFont(tbFont);	
	tbgrp->connect(tbDecPushButton, SIGNAL( clicked() ),
		       this, SLOT( decTbEvent() ) );
	tbLayout->addWidget(tbDecPushButton);

	tbInfoTextEdit = new QTextEdit(tbgrp);
	tbInfoTextEdit->setFont (tbFont);
	QFontMetrics metricsTb(tbFont);
	tbInfoTextEdit->setMaximumHeight ( commentMetrics.height() * 1.5 );
	tbInfoTextEdit->setMaximumWidth ( commentMetrics.width('X') * 13 );
	tbInfoTextEdit->setReadOnly(true);
	tbInfoTextEdit->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
	tbLayout->addWidget(tbInfoTextEdit);

	tbResetPushButton = new QPushButton( "clear" );
	tbResetPushButton->setStyleSheet("background-color: white;border-style:outset;border-width: 2px;border-color: black;font: bold 10px;padding: 4px;");
	tbResetPushButton->setMaximumSize ( tbMetrics.width("restart ") ,  
					  tbMetrics.height() );
	tbResetPushButton->setFont(tbFont);	
	tbgrp->connect(tbResetPushButton, SIGNAL( clicked() ),
		       this, SLOT( resetTbEvent() ) );
	tbLayout->addWidget(tbResetPushButton);

	tbgrp->setLayout(tbLayout);
	restLayout->addWidget(tbgrp);

	QGroupBox *statusgrp = new QGroupBox;
	QHBoxLayout *statusLayout = new QHBoxLayout;

	char status[256];
	sprintf(status,"# of devs: %d, Fs: %d Hz, lp: %1.1f Hz, hp: %1.1f Hz",
		comediScope->getNcomediDevices(),
		comediScope->getActualSamplingRate(),
		lpFreq,hpFreq);
	statusLabel = new QLabel(status);
	statusLayout->addWidget(statusLabel);
	statusgrp->setLayout(statusLayout);
	restLayout->addWidget(statusgrp);

	restGroup->setLayout(restLayout);
	controlLayout->addWidget(restGroup);
	restGroup->setSizePolicy ( QSizePolicy(QSizePolicy::Fixed,
						QSizePolicy::Fixed ) );

	controlBox->setLayout(controlLayout);

	comediScope->setMinimumWidth ( 400 );
	comediScope->setMinimumHeight ( 200 );

	scopeLayout->addWidget(comediScope);
	scopeGroup->setLayout(scopeLayout);

	allChScrollArea->setSizePolicy ( QSizePolicy(QSizePolicy::Fixed,
						     QSizePolicy::Expanding ) );
	allChScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	allChScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	mainLayout->addWidget(controlBox);
	mainLayout->addWidget(scopeGroup);

	changeTB();

	// just now just with default format
	if (filename) setFilename(filename,csv);

	comediScope->startDAQ();
}

ComediRecord::~ComediRecord() {
	QSettings settings(QSettings::IniFormat, 
			   QSettings::UserScope,
			   USBDUX_STRING,
			   PROGRAM_NAME);

	int n_devs = comediScope->getNcomediDevices();
	int channels = comediScope->getNchannels();
	settings.beginGroup(SETTINGS_CHANNELS);
	for(int n=0;n<n_devs;n++) {
		for(int i=0;i<channels;i++) {
			char tmp[128];
			sprintf(tmp,CHSETTING_FORMAT,n,i);
			settings.setValue(tmp, 
					  channel[n][i] -> getChannel() );
		}
	}
	settings.endGroup();
	delete comediScope;
}


void ComediRecord::disableControls() {
	filePushButton->setEnabled( false );
	rawCheckbox->setEnabled( false );
	int n_devs = comediScope->getNcomediDevices();
	int channels = comediScope->getNchannels();
	for(int n=0;n<n_devs;n++) {
		for(int i=0;i<channels;i++) {
			channel[n][i]->setEnabled( false );
		}
	}
}


void ComediRecord::enableControls() {
	filePushButton->setEnabled( true );
	rawCheckbox->setEnabled( true );
	int n_devs = comediScope->getNcomediDevices();
	int channels = comediScope->getNchannels();
	for(int n=0;n<n_devs;n++) {
		for(int i=0;i<channels;i++) {
			channel[n][i]->setEnabled( true );
		}
	}
}


void ComediRecord::setFilename(QString fn,int csv) {
	comediScope->setFilename(fn,csv);
	QString tmp;
	tmp="ComediRecord - datafile: "+fn;
	setWindowTitle( tmp );
	recPushButton->setEnabled( true );
}


void ComediRecord::enterFileName() {
	QFileDialog::Options options;
	QString filters(tr("space separated values (*.dat);;"
			   "space separated values (*.txt);;"
			   "comma separated values (*.csv)"
				));

	QFileDialog dialog(this);
	dialog.setFileMode(QFileDialog::AnyFile);
	dialog.setNameFilter(filters);
	dialog.setViewMode(QFileDialog::Detail);

        if (dialog.exec()) {
		QString fileName = dialog.selectedFiles()[0];
		QString extension = dialog.selectedNameFilter();
		extension=extension.mid(extension.indexOf("."),4);
                if (fileName.indexOf(extension)==-1) {
                        fileName=fileName+extension;
                }
		int isCSV = extension.indexOf("csv");
                setFilename(QString(fileName),isCSV>-1);
        }
}

// callback
void ComediRecord::recstartstop(int) 
{
  if (recPushButton->checkState()==Qt::Checked) 
    {
      comediScope->startRec();
    } 
  else 
    {
      comediScope->stopRec();
      // to force the user to enter a new filename
      recPushButton->setEnabled( false );
    }
}

void ComediRecord::incTbEvent() {
	if (tb_us<1000000) {
		char tmp[30];
		sprintf(tmp,"%d",tb_us);
		int base=tmp[0]-'0';
		switch (base) {
		case 1:
			tb_us=tb_us*2;
			break;
		case 2:
			tb_us=tb_us/2*5;
			break;
		case 5:
			tb_us=tb_us*2;
			break;
		default:
			tb_us=tb_us+((int)pow(10,floor(log10(tb_us))));
		}
		changeTB();
	}
}


void ComediRecord::decTbEvent() {
	int minTBvalue = 1000000 / comediScope->getActualSamplingRate();
	if (minTBvalue < 1) minTBvalue = 1;
	if (tb_us > minTBvalue) {
		char tmp[30];
		sprintf(tmp,"%d",tb_us);
		int base=tmp[0]-'0';
		switch (base) {
		case 5:
			tb_us=tb_us/5*2;
			break;
		case 2:
			tb_us=tb_us/2;
			break;
		case 1:
			tb_us=tb_us/5;
			break;
		default:
			tb_us=tb_us-base;
		}
		changeTB();
	}
}


void ComediRecord::changeTB() {
	QString s;
	if (tb_us<1000) {
		s.sprintf( "%d usec", tb_us);
	} else if (tb_us<1000000) {
		tb_us = (tb_us / 1000) * 1000;
		s.sprintf( "%d msec", tb_us/1000);
	} else {
		tb_us = (tb_us / 1000000) * 1000000;
		s.sprintf( "%d sec", tb_us/1000000);
	}		
	tbInfoTextEdit->setText(s);
	comediScope->setTB(tb_us);
}

void ComediRecord::resetTbEvent() {
	    comediScope->clearScreen();
};


int main( int argc, char **argv )
{
	int c;
	int num_of_channels = 0;
	int num_of_devices = 16;
	const char *filename = NULL;
	float notch = 50;
	int port = 0;
	int sampling_rate = 1000;
	int first_dev_no = 0;
	int csv = 0;
	int fftdevno = -1;
	int fftch = -1;
	int fftmaxf = -1;
	float lpFreq = 20;
	float hpFreq = 1;
	int ignoreSettings = 0;
	const char* defaultTextStringForMissingExtData = NULL;

	QSettings settings(QSettings::IniFormat, 
			   QSettings::UserScope,
			   USBDUX_STRING,
			   PROGRAM_NAME);



	for(int i = 0;i<argc;i++) {
		if (strstr(argv[i],"-i")) ignoreSettings = 1;	
	}

	if (!ignoreSettings) {
		settings.beginGroup(SETTINGS_GLOBAL);
		num_of_channels = settings.value("num_of_channels",0).toInt();
		num_of_devices = settings.value("num_of_devices",16).toInt();
		sampling_rate = settings.value("sampling_rate",1000).toInt();
		first_dev_no = settings.value("first_dev_no",0).toInt();
		notch = settings.value("notch",50).toFloat();
		lpFreq = settings.value("lowpass",20).toFloat();
		hpFreq = settings.value("highpass",1).toFloat();
		csv = settings.value("csv",0).toInt();
		settings.endGroup();
	}
	
	QApplication a( argc, argv );		// create application object

	while (-1 != (c = getopt(argc, argv, "a:b:x:l:t:r:d:p:f:c:n:hvi"))) {
		switch (c) {
		case 'x':
			sscanf(optarg,"%d,%d,%d",&fftdevno,&fftch,&fftmaxf);
			break;
		case 'f':
			csv = atoi(optarg);
			break;
		case 'c':
			num_of_channels = strtoul(optarg,NULL,0);
			break;
		case 'd':
			num_of_devices = atoi(optarg);
			break;
		case 'l':
			first_dev_no = atoi(optarg);
			break;
		case 'r':
			sampling_rate = atoi(optarg);
			break;
		case 'n':
			notch = atof(optarg);
			break;
		case 'a':
			lpFreq = atof(optarg);
			break;
		case 'b':
			hpFreq = atof(optarg);
			break;
		case 'p':
			port = atof(optarg);
			break;
		case 't':
			defaultTextStringForMissingExtData = optarg;
			break;
		case 'i':
			break;
		case 'h':
		default:
		printf("%s usage:\n"
                       "   -f <ASCII data format: 0=space separated, 1=csv>\n"
                       "   -c <number of channels>\n"
		       "   -m <number of channel tickbox-rows>\n"
                       "   -n <notch_frequency> \n"
		       "   -d <max number of comedi devices>\n"
		       "   -l <lowest comedi device number used>\n"
                       "   -r <sampling rate for the data files> \n"
		       "   -p <TCP port for receiving external data>\n"
		       "   -t <default outp when external data hasn't been rec'd>\n"
		       "   -x <device,channel,max_freq> gives the Fequency spectrum of this dev,channel,max frequency\n"
		       "   -a lowpass frequency\n"
		       "   -b highpass frequency\n"
		       "   -h prints this help screen\n",
		       argv[0]);
		exit(1);
		}
	}

	if (optind < argc) {
		filename = argv[optind];
	}

	settings.beginGroup(SETTINGS_GLOBAL);
	settings.setValue("num_of_channels",num_of_channels);
	settings.setValue("num_of_devices",num_of_devices);
	settings.setValue("sampling_rate",sampling_rate);
	settings.setValue("first_dev_no",first_dev_no);
	settings.setValue("notch",notch);
	settings.setValue("lowpass",lpFreq);
	settings.setValue("highpass",hpFreq);
	settings.setValue("csv",csv);
	settings.endGroup();

	ComediRecord comediRecord(0,
				  ignoreSettings,
				  num_of_channels,
				  notch,
				  port,
				  num_of_devices,
				  first_dev_no,
				  sampling_rate,
				  defaultTextStringForMissingExtData,
				  filename,
				  csv,
				  fftdevno,
				  fftch,
				  fftmaxf,
				  lpFreq,
				  hpFreq
		);

	// show widget
	comediRecord.show();
	// run event loop
	return a.exec();
}
