/**
 * comediscope.h
 * (c) 2004-2013 Bernd Porr, no warranty, GNU-public license
 **/
#include <QTimer>
#include <QPainter>
#include <QApplication>
#include <QTimerEvent>
#include <QPaintEvent>

#include<sys/ioctl.h>
#include<stdio.h>
#include<fcntl.h>
#include<unistd.h>
#include<stdlib.h>

#include <comedilib.h>
#include <fcntl.h>


#include "comediscope.h"

ComediScope::ComediScope( ComediRecord *comediRecordTmp, 
			  int channels, 
			  float f, 
			  int port_for_ext_data, 
			  int maxComediDevs,
			  int first_dev_no,
			  int req_sampling_rate,
			  const char* defaultTextStringForMissingExtData,
			  int fftdevnumber, int fftchannel, int fftmaxf
	)
    : QWidget( comediRecordTmp ) {

	channels_in_use = channels;

	tb_init=1;
	tb_counter=tb_init;
	comediRecord=comediRecordTmp;
	// erase plot
	eraseFlag = 1;

	fftdevno = fftdevnumber;
	fftch = fftchannel;
	fftmaxfrequency = fftmaxf;

	// for ASCII
	rec_file=NULL;

	// filename
	rec_filename=new QString();

	// flag if data has been recorded and we need a new filename
	recorded=0;

	if (port_for_ext_data>0) {
		fprintf(stderr,
			"Expecting a connection on TCP port %d. \n"
			"Start your client now, for example: \n"
			"telnet localhost %d\n"
			"Press Ctrl-C to abort.\n",
			port_for_ext_data,
			port_for_ext_data);
		ext_data_receive = new Ext_data_receive(
			port_for_ext_data,
			defaultTextStringForMissingExtData
			);
	} else {
		ext_data_receive = NULL;
	}	


	//////////////////////////////////////////////////////////////

	setAttribute(Qt::WA_OpaquePaintEvent);

	int range = 0;
	int aref = AREF_GROUND;
	// do not produce NAN for out of range behaviour
	comedi_set_global_oor_behavior(COMEDI_OOR_NUMBER);

	// create an array which keeps the comedi devices
	dev = new comedi_t*[maxComediDevs];
	for(int devNo=0;devNo<maxComediDevs;devNo++) {
		dev[devNo] = NULL;
	}
	// let's probe how many we have
	nComediDevices = 0;
	for(int devNo=0;devNo<maxComediDevs;devNo++) {
		char filename[128];
		sprintf(filename,"/dev/comedi%d",devNo+first_dev_no);
		dev[devNo] = comedi_open(filename);
		if(dev[devNo]){
			nComediDevices = devNo + 1;
		} else {
			break;
		}
	}

	// none detected
	if (nComediDevices<1) {
		fprintf(stderr,"No comedi devices detected!\n");
		exit(1);
	}

	// create channel lists
	chanlist = new unsigned int*[nComediDevices];
	// create command structures
	cmd = new comedi_cmd*[nComediDevices];
	// find the subdevice which is analogue in
	subdevice = comedi_find_subdevice_by_type(dev[0],COMEDI_SUBD_AI,0);

	// check if user has specified channels or if requested
	// number of channels make sense
	if ((channels_in_use < 1)||
	    (channels_in_use > comedi_get_n_channels(dev[0],subdevice)))
	{
		channels_in_use = comedi_get_n_channels(dev[0],subdevice);
	}

	// create channel lists and the command structures
	for(int devNo=0;devNo<nComediDevices;devNo++) {
		chanlist[devNo] = new unsigned int[channels_in_use];
		assert( chanlist[devNo]!=NULL );
		for(int i=0;i<channels_in_use;i++){
			chanlist[devNo][i] = CR_PACK(i,range,aref);
		}
		cmd[devNo] = new comedi_cmd;
		assert( dev[devNo]!=NULL );
		int r = comedi_get_cmd_generic_timed(dev[devNo],
						     subdevice,
						     cmd[devNo],
						     channels_in_use,
						     (int)(1e9/req_sampling_rate));
		if(r<0){
			comedi_perror("comedi_get_cmd_generic_timed failed\n");
			exit(-1);
		}
		/* Modify parts of the command */
		cmd[devNo]->chanlist           = chanlist[devNo];
		cmd[devNo]->chanlist_len       = channels_in_use;
		cmd[devNo]->scan_end_arg = channels_in_use;
		cmd[devNo]->stop_src=TRIG_NONE;
		cmd[devNo]->stop_arg=0;
		int ret = comedi_command_test(dev[devNo],cmd[devNo]);
		if(ret<0){
			comedi_perror("1st comedi_command_test failed\n");
			exit(-1);
		}
		ret = comedi_command_test(dev[devNo],cmd[devNo]);
		if(ret<0){
			comedi_perror("2nd comedi_command_test failed\n");
			exit(-1);
		}
		if(ret!=0){
			fprintf(stderr,"Error preparing command.\n");
			exit(-1);
		}
	}

	// the timing is done channel by channel
	// this means that the actual sampling rate is divided by
	// number of channels
	if ((cmd[0]->convert_src ==  TRIG_TIMER)&&(cmd[0]->convert_arg)) {
		sampling_rate=((1E9 / cmd[0]->convert_arg)/channels_in_use);
	}
	
	// the timing is done scan by scan (all channels at once)
	// the sampling rate is equivalent of the scan_begin_arg
	if ((cmd[0]->scan_begin_src ==  TRIG_TIMER)&&(cmd[0]->scan_begin_arg)) {
		sampling_rate=1E9 / cmd[0]->scan_begin_arg;
	}

	// initialise the graphics stuff
	ypos = new int**[nComediDevices];
	assert(ypos != NULL);
	for(int devNo=0;devNo<nComediDevices;devNo++) {
		ypos[devNo]=new int*[channels_in_use];
		assert(ypos[devNo] != NULL);
		for(int i=0;i<channels_in_use;i++) {
			ypos[devNo][i] = new int[MAX_DISP_X];
			assert( ypos[devNo][i] != NULL);
			for(int j=0;j<MAX_DISP_X;j++) {
				ypos[devNo][i][j]=0;
			}
		}
	}

	xpos=0;
	nsamples=0;

	maxdata = new lsampl_t[nComediDevices];
	assert( maxdata != NULL );
	crange = new comedi_range*[nComediDevices];
	assert( crange != NULL );
	for(int devNo=0;devNo<nComediDevices;devNo++) {
		// we just go for the default ranges
		maxdata[devNo]=comedi_get_maxdata(dev[devNo],subdevice,0);
		crange[devNo]=comedi_get_range(dev[devNo],subdevice,0,0);
	}

	// 50Hz or 60Hz mains notch filter
	iirnotch = new Iir::Butterworth::BandStop<IIRORDER>*[nComediDevices];
	assert( iirnotch != NULL );
	adAvgBuffer = new float*[nComediDevices];
	assert( adAvgBuffer != NULL );
	daqData = new lsampl_t*[nComediDevices];
	assert( daqData != NULL );
	for(int devNo=0;devNo<nComediDevices;devNo++) {
		iirnotch[devNo] = new Iir::Butterworth::BandStop<IIRORDER>[channels_in_use];
		assert( iirnotch[devNo] != NULL );
		// floating point buffer for plotting
		adAvgBuffer[devNo]=new float[channels_in_use];
		assert( adAvgBuffer[devNo] != NULL );
		for(int i=0;i<channels_in_use;i++) {
			adAvgBuffer[devNo][i]=0;
		}
		// raw data buffer for saving the data
		daqData[devNo] = new lsampl_t[channels_in_use];
		assert( daqData[devNo] != NULL );
		for(int i=0;i<channels_in_use;i++) {
			daqData[devNo][i]=0;
		}
	}

	setNotchFrequency(f);

	counter = new QTimer( this );
	assert( counter != NULL );
	connect( counter, 
		 SIGNAL(timeout()),
		 this, 
		 SLOT(updateTime()) );
}


void ComediScope::startDAQ() {
	for(int devNo=0;devNo<nComediDevices;devNo++) {
		/* start the command */
		int ret=comedi_command(dev[devNo],cmd[devNo]);
		if(ret<0){
			comedi_perror("comedi_command");
			exit(1);
		}
	}
	startTimer( 50 );		// run continuous timer
	counter->start( 500 );
}


ComediScope::~ComediScope() {
	if (rec_file) {
		fclose(rec_file);
	}
}


void ComediScope::setNotchFrequency(float f) {
	if (f>sampling_rate) {
		fprintf(stderr,
			"Error: The notch frequency %f "
			"is higher than the sampling rate of %dHz.\n",
			f,sampling_rate);
		return;
	}
	for(int j=0;j<nComediDevices;j++) {
		for(int i=0;i<channels_in_use;i++) {
			iirnotch[j][i].setup(sampling_rate,f,BS_BANDWIDTH); 
		}
		notchFrequency = f;
	}
}



void ComediScope::updateTime() {
	QString s;
	if (!rec_file) {
		if (rec_filename->isEmpty()) {
			s.sprintf("comedirecord");
		} else {
			if (recorded) {
				s=(*rec_filename)+" --- file saved";
			} else {
				s=(*rec_filename)+" --- press REC to record ";
			}
		}
	} else {
		s = (*rec_filename) + 
			QString().sprintf("--- rec: %ldsec", nsamples/sampling_rate );
	}
	comediRecord->setWindowTitle( s );

	char tmp[256];
	for(int n=0;n<nComediDevices;n++) {
		for(int i=0;i<channels_in_use;i++) {
			float phys=comedi_to_phys(daqData[n][i],
						  crange[n],
						  maxdata[n]);
			
			sprintf(tmp,"" VOLT_FORMAT_STRING "",phys);
			comediRecord->voltageTextEdit[n][i]->setText(tmp);
		}
	}
}


void ComediScope::setFilename(QString name,int csv) {
	(*rec_filename)=name;
	recorded=0;
	if (csv) {
		separator=',';
	} else {
		separator=' ';
	}
}


void ComediScope::writeFile() {
	if (!rec_file) return;
	if (comediRecord->
	    rawCheckbox->isChecked()) {
		fprintf(rec_file,"%ld",nsamples);
	} else {
		fprintf(rec_file,"%f",((float)nsamples)/((float)sampling_rate));
	}
	for(int n=0;n<nComediDevices;n++) {
		for(int i=0;i<channels_in_use;i++) {
			if (comediRecord->
			    channel[n][i]->isActive()
				) {
				if (comediRecord->
				    rawCheckbox->isChecked()) {
					fprintf(rec_file,
						"%c%d",separator,(int)(daqData[n][i]));
				} else {
					float phy=comedi_to_phys((lsampl_t)(daqData[n][i]),
								 crange[n],
								 maxdata[n]);
					fprintf(rec_file,"%c%f",separator,phy);
				}
			}
		}
	}
	if (ext_data_receive) {
		fprintf(rec_file,"%c%s",separator,ext_data_receive->getData());
	}
	fprintf(rec_file,"\n");
}

void ComediScope::startRec() {
	if (recorded) return;
	if (rec_filename->isEmpty()) return;
	comediRecord->disableControls();
	// counter for samples
	nsamples=0;
	// get possible comments
	QString comment = comediRecord->commentTextEdit->toPlainText();
	// ASCII
	rec_file=NULL;
	// do we have a valid filename?
	if (rec_filename)
		rec_file=fopen(rec_filename->toLocal8Bit().constData(),
			       "wt");
	// could we open it?
	if (!rec_file) {
		// could not open
		delete rec_filename;
		// print error msg
		fprintf(stderr,
			"Writing to %s failed.\n",
			rec_filename->toLocal8Bit().constData());
	}
	// print comment
	if ((rec_file)&&(!comment.isEmpty())) {
		fprintf(rec_file,
			"# %s\n",
			comment.toLocal8Bit().constData());
	}
}


void ComediScope::stopRec() {
	if (rec_file) {
		fclose(rec_file);
		rec_file = NULL;
		recorded = 1;
	}
	// re-enabel channel switches
	comediRecord->enableControls();
	// we should have a filename, get rid of it and create an empty one
	if (rec_filename) delete rec_filename;
	rec_filename = new QString();
}



void ComediScope::paintData(float** buffer) {
	QPainter paint( this );
	QPen penData[3]={QPen(Qt::blue,1),
			 QPen(Qt::green,1),
			 QPen(Qt::red,1)};
	QPen penWhite(Qt::white,2);
	int w = width();
	int h = height();
	if (eraseFlag) {
		paint.fillRect(0,0,w,h,QColor(255,255,255));
		eraseFlag = 0;
		xpos = 0;
	}
	num_channels=0;

	for(int n=0;n<nComediDevices;n++) {
		for(int i=0;i<channels_in_use;i++) {
			if (comediRecord->channel[n][i]->isActive()) {
				num_channels++;	
			}
		}
	}
	if (!num_channels) {
		return;
	}
	int base=h/num_channels;
	if(w <= 0 || h <= 0) 
		return;
	paint.setPen(penWhite);
	int xer=xpos+5;
	if (xer>=w) {
		xer=xer-w;
	}
	paint.drawLine(xer,0,
		       xer,h);
	int act=1;
	for(int n=0;n<nComediDevices;n++) {
		float minV = crange[n]->min;
		float maxV = crange[n]->max;
		float dy=(float)base/(float)(maxV-minV);
		for(int i=0;i<channels_in_use;i++) {
			if (comediRecord->
			    channel[n][i]->
			    isActive()) {
				paint.setPen(penData[act%3]);
				float gain=comediRecord->gain[n][i]->getGain();
				float value = buffer[n][i] * gain;
				int yZero=base*act-(int)((0-minV)*dy);
				int yTmp=base*act-(int)((value-minV)*dy);
				ypos[n][i][xpos+1]=yTmp;
				paint.drawLine(xpos,ypos[n][i][xpos],
					       xpos+1,ypos[n][i][xpos+1]);
				if (xpos%2) {
					paint.drawPoint(xpos,yZero);
				}
				if ((xpos+2)==w) {
					ypos[n][i][0]=yTmp;
				}
				act++;
			}
		}
	}
	xpos++;
	if ((xpos+1)>=w) {
		xpos=0;
	}
}



//
// Handles paint events for the ComediScope widget.
// When the paint-event is triggered the averaging is done, the data is
// displayed and saved to disk.
//

void ComediScope::paintEvent( QPaintEvent * ) {
        int ret;
	while (1) {
		// we need data in all the comedi devices
		for(int n=0;n<nComediDevices;n++) {
			if (!comedi_get_buffer_contents(dev[n],subdevice))
				return;
		}

		for(int n=0;n<nComediDevices;n++) {
			int subdev_flags = comedi_get_subdevice_flags(dev[n],
								      subdevice);
			int bytes_per_sample;
			if(subdev_flags & SDF_LSAMPL) {
				bytes_per_sample = sizeof(lsampl_t);
			} else {
				bytes_per_sample = sizeof(sampl_t);
			}
			
			unsigned char buffer[bytes_per_sample*channels_in_use];
			ret = read(comedi_fileno(dev[n]),
				   buffer,
				   bytes_per_sample*channels_in_use);

			if (ret==0) {
				printf("BUG! No data in buffer.\n");
				exit(1);
			}

			if (ret<0) {
				printf("\n\nError %d during read! Exiting.\n\n",ret);
				exit(1);
			}
			
			for(int i=0;i<channels_in_use;i++) {
				int sample;
				if (comediRecord->channel[n][i]->isActive()) {
					int ch = comediRecord->channel[n][i]->getChannel();
					if(subdev_flags & SDF_LSAMPL) {
						sample = ((int)((lsampl_t *)buffer)[ch]);
					} else {
						sample = ((int)((sampl_t *)buffer)[ch]);
					}
					// store raw data
					daqData[n][i] = sample;
					// convert data to physical units for plotting
					float value = comedi_to_phys(sample,
								     crange[n],
								     maxdata[n]);
					// filtering
					value = comediRecord->dcSub[n][i]->filter(value);
					value = comediRecord->hp[n][i]->filter(value);
					value = comediRecord->lp[n][i]->filter(value);
					// remove 50Hz
					if (comediRecord->filterCheckbox->checkState()==Qt::Checked) {
						value=iirnotch[n][i].filter(value);
					}
					if ((n==fftdevno) && (ch==fftch) &&
					    (comediRecord->fftscope))
						comediRecord->fftscope->append(value);
					// average response if TB is slower than sampling rate
					adAvgBuffer[n][i] = adAvgBuffer[n][i] + value;
				}
			}
		}

		// save data
		if (comediRecord->recPushButton->checkState()==Qt::Checked) {
			writeFile();
		}

		nsamples++;
		tb_counter--;

		// enough averaged?
		if (tb_counter<=0) {
			for(int n=0;n<nComediDevices;n++) {
				for(int i=0;i<channels_in_use;i++) {
					adAvgBuffer[n][i]=adAvgBuffer[n][i]/tb_init;
				}
			}
		
			// plot the stuff
			paintData(adAvgBuffer);

			// clear buffer
			tb_counter=tb_init;
			for(int n=0;n<nComediDevices;n++) {
				for(int i=0;i<channels_in_use;i++) {
					adAvgBuffer[n][i]=0;
				}
			}
		}
	}
}


void ComediScope::setTB(int us) {
	tb_init=us/(1000000/sampling_rate);
	tb_counter=tb_init;
	for(int n=0;n<nComediDevices;n++) {
		for(int i=0;i<channels_in_use;i++) {
			adAvgBuffer[n][i]=0;
		}
	}
}

//
// Handles timer events for the ComediScope widget.
//

void ComediScope::timerEvent( QTimerEvent * )
{
	if (ext_data_receive) {
		ext_data_receive->readSocket();
	}
	repaint();
}

void ComediScope::clearScreen()
{
	eraseFlag = 1;
	repaint();
}
