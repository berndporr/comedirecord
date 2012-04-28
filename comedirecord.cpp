/********************************************************************
 * comedirecord.cpp 
 * License: GNU, GPL
 * (c) 2004-2011, Bernd Porr
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

// for the layout
#define MAXROWS 8

ComediRecord::ComediRecord( QWidget *parent, 
			    int nchannels,
			    float notchF,
			    int port,
			    int num_of_devices,
			    int requrested_sampling_rate,
			    const char* defaultTextStringForMissingExtData,
			    const char* filename
	)
    : QWidget( parent ) {

        comediScope=new ComediScope(this,
				    nchannels,
				    notchF,
				    port,
				    num_of_devices,
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
			hbox[n][i]->setSpacing(2);
			channelCheckbox[n][i] = new QCheckBox;
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
			voltageTextEdit[n][i]->setMaximumSize
				(voltageMetrics.width("+4.00000000V "),
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
			if (row > MAXROWS) {
				row = 1;
				column++;
			}
		}
		column++;
		row = 1;
	}
	controlLayout->addWidget(allChGroup);

	// at least one should be active not to make the user nervous.
	channelCheckbox[0][0]->setChecked( TRUE );

	// notch filter
	// create a group for the notch filter
	QGroupBox* notchGroupBox = new QGroupBox();
	notchGroupBox->setFlat(TRUE);
	notchGroupBox->setStyleSheet(styleSheet);
	QHBoxLayout *notchLayout = new QHBoxLayout();
	char tmp[128];
	sprintf(tmp,"%2.0f Hz notch filter",notchF);
	filterCheckbox=new QCheckBox( tmp );
	filterCheckbox->setChecked( FALSE );
	notchLayout->addWidget(filterCheckbox);
       	notchGroupBox->setLayout(notchLayout);

	controlLayout->addWidget(notchGroupBox);

	// group for the record stuff
	QGroupBox* recGroupBox = new QGroupBox("Write to file");
	QHBoxLayout *recLayout = new QHBoxLayout();

	rawCheckbox=new QCheckBox("raw data" );
	rawCheckbox->setChecked( false );
	recLayout->addWidget(rawCheckbox);

	filePushButton = new QPushButton( "&filename" );
	connect(filePushButton, SIGNAL( clicked() ),
		this, SLOT( enterFileName() ) );
	recLayout->addWidget(filePushButton);

	recPushButton = new QCheckBox( "&REC" );
	recPushButton->connect(recPushButton, SIGNAL( stateChanged(int) ),
			       this, SLOT( recstartstop(int) ) );
	recPushButton->setEnabled( false );
	recLayout->addWidget(recPushButton);

	recGroupBox->setLayout(recLayout);
	controlLayout->addWidget(recGroupBox);

	// group for the time base
	QGroupBox *tbgrp = new QGroupBox("Timebase");
	QHBoxLayout *tbLayout = new QHBoxLayout;
	QFont tbFont("Courier",12);
	tbFont.setBold(TRUE);
	QFontMetrics tbMetrics(tbFont);

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
	tbInfoTextEdit->setMaximumSize (3*metricsTb.width("99999 sec")/2,
					3*metricsTb.height()/2);
	tbInfoTextEdit->setReadOnly(TRUE);
	tbInfoTextEdit->setLineWidth(1);
	tbInfoTextEdit->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
	tbLayout->addWidget(tbInfoTextEdit);

	tbgrp->setLayout(tbLayout);
	controlLayout->addWidget(tbgrp);
	controlBox->setLayout(controlLayout);

	comediScope->setMinimumWidth ( 300 );
	comediScope->setMinimumHeight ( 200 );

	scopeLayout->addWidget(comediScope);
	scopeGroup->setLayout(scopeLayout);

	controlBox->setSizePolicy ( QSizePolicy(QSizePolicy::Fixed,
						QSizePolicy::Fixed ) );

	mainLayout->addWidget(controlBox);
	mainLayout->addWidget(scopeGroup);

	changeTB();

	if (filename) setFilename(filename,0);

	comediScope->startDAQ();
}

ComediRecord::~ComediRecord() {
	delete comediScope;
}


void ComediRecord::setFilename(QString fn,int csv) {
	if (comediScope->setFilename(fn,csv)==-1) {
		return;
	}
	QString tmp;
	tmp="ComediRecord - datafile: "+fn;
	setWindowTitle( tmp );
	recPushButton->setEnabled( true );
}


void ComediRecord::enterFileName() {
	QFileDialog::Options options;
	QString filters(tr("space separated values (*.txt);;"
			   "space separated values (*.dat);;"
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

void ComediRecord::recstartstop(int) 
{
  if (recPushButton->checkState()==Qt::Checked) 
    {
      comediScope->startRec();
    } 
  else 
    {
      comediScope->stopRec();
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
	int num_of_devices = 16;
	const char *filename = NULL;
	float notch = 50;
	int port = 0;
	int sampling_rate = 1000;
	const char* defaultTextStringForMissingExtData = NULL;

	QApplication a( argc, argv );		// create application object

	while (-1 != (c = getopt(argc, argv, "t:r:d:p:f:c:n:h"))) {
		switch (c) {
		case 'f':
			filename = optarg;
			break;
		case 'c':
			num_of_channels = strtoul(optarg,NULL,0);
			break;
		case 'd':
			num_of_devices = atoi(optarg);
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
		case 'h':
		default:
		printf("%s usage:\n"
                       "   -f <filename for data>\n"
                       "   -c <number of channels>\n"
                       "   -n <notch_frequency> \n"
		       "   -d <max number of comedi devices>\n"
                       "   -r <sampling rate> \n"
		       "   -p <TCP port for receiving external data>\n"
		       "   -t <default outp when external data hasn't been rec'd>\n"
		       "\n",argv[0]);
		exit(1);
		}
	}

	ComediRecord comediRecord(0,
				  num_of_channels,
				  notch,
				  port,
				  num_of_devices,
				  sampling_rate,
				  defaultTextStringForMissingExtData,
				  filename
		);

	comediRecord.show();			// show widget
	return a.exec();			// run event loop
}
