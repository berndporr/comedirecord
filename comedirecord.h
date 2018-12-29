/**
 * comedirecord.h
 * (c) 2004-2018, Bernd Porr, no warranty, GNU-public license
 * mail@berndporr.me.uk
 **/
class ComediRecord;
#ifndef COMEDIRECORD_H
#define COMEDIRECORD_H

#include <QWidget>
#include <QPainter>
#include <QApplication>
#include <QPushButton>
#include <QCheckBox>
#include <QLayout> 
#include <QTextEdit>
#include <QGroupBox>
#include <QLabel>

#include "comediscope.h"
#include "gain.h"
#include "channel.h"
#include "dc_sub.h"
#include "hp.h"
#include "lp.h"
#include "fftscope.h"

// defines how quickly the DC detector follows the signal
// the larger the value the slower
#define INERTIA_FOR_DC_DETECTION 1000

class ComediRecord : public QWidget
{

    Q_OBJECT
public:
/**
 * Constructor
 **/
	ComediRecord( QWidget *parent,
		      int ignoreSettings = 0,
		      int channels = 1, 
		      float notch = 50.0,
		      int port = 0,
		      int num_of_devices = 1,
		      int first_dev_no = 0,
		      int requested_sampling_rate = 1000,
		      const char* defaultTextStringForMissingExtData = NULL,
		      const char* filename = NULL,
		      int csv = 0,
		      int fftdevnumber = -1,
		      int fftchannel = -1,
		      int fftmaxf = -1,
		      float lpFreq = 10,
		      float hpFreq = 1
		);
	
	/**
	 * Destructor
	 **/
	~ComediRecord();
	
public:
/**
 * Sets the filename for the data-file
 **/
	void setFilename(QString fn,int csv);

public:
	void enableControls();
	void disableControls();

public:

/**
 * Button which controls recording
 **/
    QCheckBox *recPushButton;

/**
 * channel label
 **/
    QLabel*** channelLabel;

    QLabel*** subDClabel;

    QLabel*** hpLabel;

    QLabel*** lpLabel;

    /**
     * Notch filter on?
     **/
    QCheckBox* filterCheckbox;

public:
    /**
     * Comments
     **/
    QTextEdit* commentTextEdit;

    /**
     * Raw data checkbox
     **/
    QCheckBox* rawCheckbox;

    /**
     * Shows the filter frequency
     **/
    QTextEdit*  filterTextEdit;

    /**
     * Array of the voltages
     **/
    QTextEdit***  voltageTextEdit;

    /**
     * Array for the gain settings
     **/
    Channel*** channel;

    /**
     * Array for the gain settings
     **/
    Gain*** gain;

    /**
     * subtracting DC
     **/
    DCSub*** dcSub;

    /**
     * highpass filter
     **/
    Hp*** hp;

    /**
     * lowpass filter
     **/
    Lp*** lp;

    /**
     * The widget which contains the graphical plots of the AD-data
     **/
    ComediScope* comediScope;

    /**
     * Text-field: elapsed time
     **/
    QTextEdit* timeInfoTextEdit;

    /**
     * Text-field for the file-name
     **/
    QPushButton *filePushButton;

 private:
    /**
     * Button: Increase the time between samples
     **/
    QPushButton *tbIncPushButton;

    /**
     * Button: Decrease the time between samples
     **/
    QPushButton *tbDecPushButton;

    /**
     * Button: Reset screen
     **/
    QPushButton *tbResetPushButton;

/**
 * Text-field: time between samples
 **/
    QTextEdit   *tbInfoTextEdit;

/**
 * Button: clears the screen
 **/
    QPushButton *clearScreenPushButton;

private slots:
/**
 * Button to increase the time-base has been pressed
 **/
    void incTbEvent();

/**
 * Reset screen
 **/
    void resetTbEvent(); 

/**
 * Button to decrease the time-base has been pressed
 **/
    void decTbEvent();
    
    /**
     * Enter a new filename
     **/
    void enterFileName();

    /**
     * Pressing or releasing the record button
     **/
    void recstartstop(int);

private:
/**
 * Called if a change in the time-base has occurred
 **/
    void changeTB();

/**
 * returns the timebase
 **/
public:
    int getTB() {return tb_us;};

private:
/**
 * Time between two samples in ms
 **/
    int tb_us;

/**
 * Group which keeps one channel together
 **/
    QGroupBox*** channelgrp;

/**
 * Layout which keeps one channel together
 **/
    QHBoxLayout*** hbox;

    QLabel *statusLabel;

public:
/**
 * FFT window
 **/
    FFTScope *fftscope;

};


#endif
