/**
 * comediscope.h
 * (c) 2004-2012 Bernd Porr, no warranty, GNU-public license
 **/
class ComediScope;
#ifndef COMEDISCOPE_H
#define COMEDISCOPE_H

#include <QWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QLayout> 
#include <QPaintEvent>
#include <QTimerEvent>

#include <Iir.h>
#include <hdf5.h>
#include <hdf5_hl.h>

#include <comedilib.h>
#include <fcntl.h>
#include "ext_data_receive.h"

#include "comedirecord.h"

#define MAX_DISP_X 4096 // max screen width

#define IIRORDER 2

#define VOLT_FORMAT_STRING "%+.3f"

class ComediScope : public QWidget
{
    Q_OBJECT
public:
/**
 * Constructor:
 **/
    ComediScope( ComediRecord* comediRecordTmp,
		 int channels = 0,
		 float notchF = 50,
		 int port_for_ext_data = 0,
		 int maxComediDevices = 1,
		 int first_dev_no = 0,
		 int req_sampling_rate = 1000,
		 const char *defaultTextStringForMissingExtData = NULL
	    );
/**
 * Destructor: close the file if necessary
 **/
    ~ComediScope();


protected:
/**
 * Overloads the empty paint-function: draws the functions and saves the data
 **/
    void	paintEvent( QPaintEvent * );

public:
/**
 * Clears the screen
 **/
    void        clearScreen();

private:
/**
 * Paints the data on the screen, is callsed by paintEvent
 **/
    void        paintData(float** buffer);

/**
 * Is called by the timer of the window. This causes the drawing and saving
 * of all not yet displayed/saved data.
 **/
    void	timerEvent( QTimerEvent * );

private slots:

/**
 * Updates the time-code and also the voltages of the channels
 **/
    void	updateTime();

private:
/**
 * Saves data which has arrived from the AD-converter
 **/
    void        writeFile();


    void        writeASCIIFile();
    void        writeHDF5File();


private:
    /**
     * file descriptor for /dev/comedi0
     **/
    comedi_t **dev;

 private:
    unsigned int** chanlist;
    
 private:
    int subdevice;

 private:
    comedi_cmd** cmd;

    /**
     * y-positions of the data
     **/
    int         ***ypos;

    /**
     * current x-pos
     **/
    int         xpos;

    /**
     * elapsed msec
     **/
    long int         nsamples;

public:
    /**
     * sets the filename for the data-file
     **/
    void        setFilename(QString name,int csv,int hdf5);

public:
    /**
     * sets the time between the samples
     **/
    void        setTB(int us);

private:
    /**
     * pointer to the parent widget which contains all the controls
     **/
    ComediRecord* comediRecord;
  

private:
    /**
     * the filename of the data-file
     **/
    QString*     rec_filename;

    /**
     * the file descriptor to the data-file
     **/
    FILE*       rec_file;

    /**
     * HDF5 table where one row contains the data from one moment in time
     **/
    hid_t       hdf5table_id;

    /**
     * pointer to the file
     **/
    hid_t       hdf5file_id;

    /**
     * we have a valid HDF5 setup, we can record
     **/
    int         hdf5valid;

    /**
     * we record in HDF5 and not ASCII
     **/
    int         recordInHDF5;

    /**
     * buffer which stores the data
     **/
    float*      hdf5floatBuffer;

    /**
     * the type which describes the sample
     **/
    hid_t       hdf5compoundtype;

    /**
     * number of channels switched on
     **/
    int         num_channels;

private:
    /**
     * buffer which adds up the data for averaging
     **/
    float**   adAvgBuffer;

private:
    /**
     * init value for the averaging counter
     **/
    int         tb_init;

private:
    /**
     * counter for the tb. If zero the average is
     * taken from adAvgBuffer and saved into actualAD.
     **/
    int         tb_counter;

public:
    /**
     * Starts recording
     **/
    void startRec();

    /**
     * Ends recording
     **/
    void stopRec();

private:
    /**
     * the number of channels actually used per comedi device
     **/
    int channels_in_use;

public:
    int getNchannels() {return channels_in_use;};

private:
    /**
     * Frequency for the notch filter in Hz
     **/
    float notchFrequency;

public:
    /**
     * sets the notch frequency
     **/
    void setNotchFrequency(float f);

    /**
     * gets the notch frequency
     **/
    float getNotchFrequency() {
	    return notchFrequency;
    }

 private:
    /**
     * flag if data has been recorded. Prevents overwrite.
     **/
    int recorded;
    /**
     * the max value of the A/D converter
     **/
    lsampl_t* maxdata;

public:
    /**
     * physical range
     **/
    comedi_range** crange;

    /**
     * notch filter
     **/
    Iir::Butterworth::BandStop<IIRORDER>*** iirnotch;

    /**
     * comma separated?
     **/
    char separator;

/**
 * Object for external data reception via a socket
 **/
    Ext_data_receive* ext_data_receive;

/**
 * Number of detected comedi devices
 **/
    int nComediDevices;

/**
 * Timer for printing the voltages in the textfields
 **/
    QTimer* counter;

/**
 * Raw daq data from the A/D converter which is saved to a file
 **/
    lsampl_t** daqData;

/**
 * The actual sampling rate
 **/
    int sampling_rate;

public:
/**
 * Start the DAQ board(s)
 **/
    void startDAQ();

public:
/**
 * Gets the number of comedi devices actually used here
 **/
    int getNcomediDevices() {return nComediDevices;};

public:
/**
 * Gets the actual sampling rate the boards are running at.
 **/
    int getActualSamplingRate() {return sampling_rate;};

};


#endif
