/********************************************************************
 * comedirecord.cpp 
 * License: GNU, GPL
 * (c) 2004-2012, Bernd Porr
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
#define CHSETTING_FORMAT "dev%09d_ch%09d"
#define USBDUX_STRING "USB-DUX"
#define PROGRAM_NAME "comedirecord"

// for the layout
// how we organise the channels switches
#define MAXROWS 8


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
			    int nchannels,
			    int maxrows,
			    float notchF,
			    int port,
			    int num_of_devices,
			    int first_dev_no,
			    int requrested_sampling_rate,
			    const char* defaultTextStringForMissingExtData,
			    const char* filename,
			    int csv
	)
    : QWidget( parent ) {

        comediScope=new ComediScope(this,
				    nchannels,
				    notchF,
				    port,
				    num_of_devices,
				    first_dev_no,
				    requrested_sampling_rate,
				    defaultTextStringForMissingExtData
		);

	int channels = comediScope->getNchannels();

	tb_us = 1000000 / comediScope->getActualSamplingRate();

	// fonts
	QFont voltageFont("Courier",10);
	QFontMetrics voltageMetrics(voltageFont);

	// this the main layout which contains two sub-windows:
	// the control window and the oscilloscope window
	QHBoxLayout *mainLayout = new QHBoxLayout;
	mainLayout->setSpacing(0);
	setLayout(mainLayout);

	// this is the vertical layout for all the controls
	QVBoxLayout *controlLayout = new QVBoxLayout(0);
	// the corresponding box which contains all the controls
	QGroupBox *controlBox = new QGroupBox ();

	// this is the vertical layout for all the controls
	QVBoxLayout *scopeLayout = new QVBoxLayout(0);
	// the corresponding box which contains all the controls
	QGroupBox *scopeGroup = new QGroupBox ();

	QGridLayout *allChLayout = new QGridLayout;
	QGroupBox *allChGroup = new QGroupBox ();
	allChGroup->setStyleSheet("padding:0px;margin:0px;border:0px;");
	allChGroup->setLayout(allChLayout);
	allChLayout->setSpacing(1);

	int column = 1;
	int row = 1;

	int n_devs = comediScope->getNcomediDevices();

	channelLabel=new QLabel**[n_devs];
	channelCheckbox=new QCheckBox**[n_devs];
	voltageTextEdit=new QTextEdit**[n_devs];
	channelgrp=new QGroupBox**[n_devs];
	hbox=new QHBoxLayout**[n_devs];
	gain=new Gain**[n_devs];
	dcSub=new DCSub**[n_devs];
	subDClabel=new QLabel**[n_devs];
	
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
		channelCheckbox[n]=new QCheckBox*[channels];
		voltageTextEdit[n]=new QTextEdit*[channels];
		channelgrp[n]=new QGroupBox*[channels];
		hbox[n]=new QHBoxLayout*[channels];
		gain[n]=new Gain*[channels];
		dcSub[n]=new DCSub*[channels];
		subDClabel[n]=new QLabel*[channels];
		for(int i=0;i<channels;i++) {
			// create the group for a channel
			char tmp[10];
			channelgrp[n][i] = new QGroupBox();
			channelgrp[n][i]->setStyleSheet(styleSheet);
			// the corresponding layout
			hbox[n][i] = new QHBoxLayout();
			channelgrp[n][i]->setLayout(hbox[n][i]);
			sprintf(tmp,"%d",i);
			channelLabel[n][i] = new QLabel(tmp);
			channelLabel[n][i]->setStyleSheet(styleSheet);
			channelLabel[n][i]->setFont(voltageFont);
			hbox[n][i]->addWidget(channelLabel[n][i]);
			hbox[n][i]->setSpacing(1);
			channelCheckbox[n][i] = new QCheckBox;
			char tmpCh[128];
			sprintf(tmpCh,CHSETTING_FORMAT,n,i);
			channelCheckbox[n][i] -> setChecked( 
				settings.value(tmpCh,0).toBool() ); 
			if ( channelCheckbox[n][i] -> isChecked() )
				nch_enabled++;
			channelCheckbox[n][i]->setStyleSheet(styleSheet);
			hbox[n][i]->addWidget(channelCheckbox[n][i]);
			channelgrp[n][i]->connect(channelCheckbox[n][i], 
					       SIGNAL( clicked() ),
					       this, 
					       SLOT( clearEvent() ) );
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
			voltageTextEdit[n][i]->setReadOnly(TRUE);
			voltageTextEdit[n][i]->setFont(voltageFont);
			// voltageTextEdit[i]->setLineWidth(1);

			subDClabel[n][i] = new QLabel("-DC");
			subDClabel[n][i]->setStyleSheet(styleSheet);
			subDClabel[n][i]->setFont(voltageFont);
			hbox[n][i]->addWidget(subDClabel[n][i]);

			dcSub[n][i] = new DCSub((float)INERTIA_FOR_DC_DETECTION);
			dcSub[n][i]->setStyleSheet(styleSheet);
			hbox[n][i]->addWidget(dcSub[n][i]);

			gain[n][i] = new Gain();
			gain[n][i]->setStyleSheet(styleSheet);
			hbox[n][i]->addWidget(gain[n][i]);

			allChLayout->addWidget(channelgrp[n][i],row,column);

			row++;
			if (row > maxrows) {
				row = 1;
				column++;
			}
		}
		column++;
		row = 1;
	}

	settings.endGroup();

	controlLayout->addWidget(allChGroup);

	// at least one should be active not to make the user nervous.
	if (nch_enabled==0)
		channelCheckbox[0][0]->setChecked( TRUE );

	// notch filter
	// create a group for the notch filter
	QGroupBox* notchGroupBox = new QGroupBox();
	notchGroupBox->setFlat(TRUE);
	notchGroupBox->setStyleSheet(styleSheet);
	QHBoxLayout *notchLayout = new QHBoxLayout();
	char tmp[128];
	sprintf(tmp,"%2.0fHz notch",notchF);
	filterCheckbox=new QCheckBox( tmp );
	filterCheckbox->setChecked( FALSE );
	notchLayout->addWidget(filterCheckbox);
	commentTextEdit=new QCommentTextEdit();
	QFont commentFont("Courier",10);
	QFontMetrics commentMetrics(commentFont);
	commentTextEdit->setMaximumHeight ( commentMetrics.height() );
	commentTextEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	commentTextEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	commentTextEdit->setFont(commentFont);
	QLabel *l=new QLabel("| Comment:");
	notchLayout->addWidget(l);
	notchLayout->addWidget(commentTextEdit);
       	notchGroupBox->setLayout(notchLayout);

	controlLayout->addWidget(notchGroupBox);

	// group for the record stuff
	QGroupBox* recGroupBox = new QGroupBox();
	QHBoxLayout *recLayout = new QHBoxLayout();

	QLabel* recLabel = new QLabel("Write to file:");
	recLayout->addWidget(recLabel);

	filePushButton = new QPushButton( "&filename" );
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
	controlLayout->addWidget(recGroupBox);

	// group for the time base
	QGroupBox *tbgrp = new QGroupBox();
	QHBoxLayout *tbLayout = new QHBoxLayout;
	QFont tbFont("Courier",12);
	tbFont.setBold(TRUE);
	QFontMetrics tbMetrics(tbFont);

	QLabel* tbLabel = new QLabel("Timebase:");
	tbLayout->addWidget(tbLabel);

	tbIncPushButton = new QPushButton( "+" );
	tbIncPushButton->setMaximumSize ( tbMetrics.width(" + ") ,  
					  tbMetrics.height() );
	tbIncPushButton->setFont(tbFont);
	tbgrp->connect(tbIncPushButton, SIGNAL( clicked() ),
		this, SLOT( incTbEvent() ) );
	tbLayout->addWidget(tbIncPushButton);

	tbDecPushButton = new QPushButton( "-" );
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
	tbInfoTextEdit->setReadOnly(TRUE);
	tbInfoTextEdit->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
	tbLayout->addWidget(tbInfoTextEdit);

	tbgrp->setLayout(tbLayout);
	controlLayout->addWidget(tbgrp);

	QGroupBox *statusgrp = new QGroupBox;
	QHBoxLayout *statusLayout = new QHBoxLayout;

	char status[256];
	sprintf(status,"comedi devs: %d, sampling rate: %d Hz",
		comediScope->getNcomediDevices(),
		comediScope->getActualSamplingRate());
	statusLabel = new QLabel(status);
	statusLayout->addWidget(statusLabel);
	statusgrp->setLayout(statusLayout);
	controlLayout->addWidget(statusgrp);

	controlBox->setLayout(controlLayout);

	comediScope->setMinimumWidth ( 400 );
	comediScope->setMinimumHeight ( 200 );

	scopeLayout->addWidget(comediScope);
	scopeGroup->setLayout(scopeLayout);

	controlBox->setSizePolicy ( QSizePolicy(QSizePolicy::Fixed,
						QSizePolicy::Fixed ) );

	mainLayout->addWidget(controlBox);
	mainLayout->addWidget(scopeGroup);

	changeTB();

	// just now just with default format
	if (filename) setFilename(filename,csv,0);

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
					  channelCheckbox[n][i] -> isChecked() );
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
			channelCheckbox[n][i]->setEnabled( false );
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
			channelCheckbox[n][i]->setEnabled( true );
		}
	}
}


void ComediRecord::setFilename(QString fn,int csv,int hdf5) {
	comediScope->setFilename(fn,csv,hdf5);
	QString tmp;
	tmp="ComediRecord - datafile: "+fn;
	setWindowTitle( tmp );
	recPushButton->setEnabled( true );
}


void ComediRecord::enterFileName() {
	QFileDialog::Options options;
	QString filters(tr("space separated values (*.dat);;"
			   "space separated values (*.txt);;"
			   "comma separated values (*.csv);;"
			   "hierachical data format (*.hdf)"
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
		int isHDF5 = extension.indexOf("hdf");
                setFilename(QString(fileName),isCSV>-1,isHDF5>-1);
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

void ComediRecord::clearEvent() {
	comediScope->clearScreen();
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

int main( int argc, char **argv )
{
	int c;
	int num_of_channels = 0;
	int maxrows = MAXROWS;
	int num_of_devices = 16;
	const char *filename = NULL;
	float notch = 50;
	int port = 0;
	int sampling_rate = 1000;
	int first_dev_no = 0;
	int csv = 0;
	const char* defaultTextStringForMissingExtData = NULL;

	QSettings settings(QSettings::IniFormat, 
			   QSettings::UserScope,
			   USBDUX_STRING,
			   PROGRAM_NAME);

	settings.beginGroup(SETTINGS_GLOBAL);
	num_of_channels = settings.value("num_of_channels",0).toInt();
	maxrows = settings.value("maxrows",MAXROWS).toInt();
	num_of_devices = settings.value("num_of_devices",16).toInt();
	sampling_rate = settings.value("sampling_rate",1000).toInt();
	first_dev_no = settings.value("first_dev_no",0).toInt();
	notch = settings.value("notch",50).toFloat();
	csv = settings.value("csv",0).toInt();
	settings.endGroup();

	QApplication a( argc, argv );		// create application object

	while (-1 != (c = getopt(argc, argv, "m:l:t:r:d:p:f:c:n:hv"))) {
		switch (c) {
		case 'm':
			maxrows = atoi(optarg);
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
		case 'p':
			port = atof(optarg);
			break;
		case 't':
			defaultTextStringForMissingExtData = optarg;
			break;
		case 'v':
			printf("Version "VERSION"\n");
			exit(1);
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
		       "   -v prints version number\n"
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
	settings.setValue("maxrows",maxrows);
	settings.setValue("sampling_rate",sampling_rate);
	settings.setValue("first_dev_no",first_dev_no);
	settings.setValue("notch",notch);
	settings.setValue("csv",csv);
	settings.endGroup();

	ComediRecord comediRecord(0,
				  num_of_channels,
				  maxrows,
				  notch,
				  port,
				  num_of_devices,
				  first_dev_no,
				  sampling_rate,
				  defaultTextStringForMissingExtData,
				  filename,
				  csv
		);

	// show widget
	comediRecord.show();
	// run event loop
	return a.exec();
}
