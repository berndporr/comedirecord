/**
 * comediscope.h
 * (c) 2004-2012 Bernd Porr, no warranty, GNU-public license
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
			  int req_sampling_rate,
			  const char* defaultTextStringForMissingExtData
	)
    : QWidget( comediRecordTmp ) {


	setAttribute(Qt::WA_NoBackground);

	channels_in_use = channels;

	tb_init=1;
	tb_counter=tb_init;
	comediRecord=comediRecordTmp;
	rec_file=NULL;
	hdf5table_id=0;
	hdf5floatBuffer=NULL;
	hdf5valid=0;
	recordInHDF5=0;

	rec_filename=new QString();
	recorded=0;

	if (port_for_ext_data>0) {
		fprintf(stderr,
			"Expecting a connection on TCP port %d. "
			"Start your client now.\n",port_for_ext_data);
		ext_data_receive = new Ext_data_receive(
			port_for_ext_data,
			defaultTextStringForMissingExtData
			);
	} else {
		ext_data_receive = NULL;
	}	


	//////////////////////////////////////////////////////////////


	int range = 0;
	int aref = AREF_GROUND;
	comedi_set_global_oor_behavior(COMEDI_OOR_NUMBER);

	dev = new comedi_t*[maxComediDevs];
	for(int devNo=0;devNo<maxComediDevs;devNo++) {
		dev[devNo] = NULL;
	}
	nComediDevices = 0;
	for(int devNo=0;devNo<maxComediDevs;devNo++) {
		char filename[128];
		sprintf(filename,"/dev/comedi%d",devNo);
		dev[devNo] = comedi_open(filename);
		if(dev[devNo]){
			nComediDevices = devNo + 1;
		} else {
			break;
		}
	}

	if (nComediDevices<1) {
		fprintf(stderr,"No comedi devices detected!\n");
		exit(1);
	}

	chanlist = new unsigned int*[nComediDevices];
	cmd = new comedi_cmd*[nComediDevices];
	subdevice = comedi_find_subdevice_by_type(dev[0],COMEDI_SUBD_AI,0);

	if (channels_in_use == 0) {
		channels_in_use = comedi_get_n_channels(dev[0],subdevice);
	}

	for(int devNo=0;devNo<nComediDevices;devNo++) {
		chanlist[devNo] = new unsigned int[channels_in_use];
		for(int i=0;i<channels_in_use;i++){
			chanlist[devNo][i] = CR_PACK(i,range,aref);
		}
		cmd[devNo] = new comedi_cmd;
		if (!dev[devNo]) {
			fprintf(stderr,"BUG! dev[%d]=NULL\n",devNo);
			exit(1);
		}
		int r = comedi_get_cmd_generic_timed(dev[devNo],
						     subdevice,
						     cmd[devNo],
						     channels_in_use,
						     (int)(1e9/req_sampling_rate));
		if(r<0){
			printf("comedi_get_cmd_generic_timed failed\n");
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
			comedi_perror("comedi_command_test");
			exit(-1);
		}
		ret = comedi_command_test(dev[devNo],cmd[devNo]);
		if(ret<0){
			comedi_perror("comedi_command_test");
			exit(-1);
		}
		if(ret!=0){
			fprintf(stderr,"Error preparing command\n");
			exit(-1);
		}
	}

	// the timing is done channel by channel
	if ((cmd[0]->convert_src ==  TRIG_TIMER)&&(cmd[0]->convert_arg)) {
		sampling_rate=((1E9 / cmd[0]->convert_arg)/channels_in_use);
	}
	
	// the timing is done scan by scan (all channels at once)
	if ((cmd[0]->scan_begin_src ==  TRIG_TIMER)&&(cmd[0]->scan_begin_arg)) {
		sampling_rate=1E9 / cmd[0]->scan_begin_arg;
	}

	ypos = new int**[nComediDevices];
	for(int devNo=0;devNo<nComediDevices;devNo++) {
		ypos[devNo]=new int*[channels_in_use];
		for(int i=0;i<channels_in_use;i++) {
			ypos[devNo][i] = new int[MAX_DISP_X];
			for(int j=0;j<MAX_DISP_X;j++) {
				ypos[devNo][i][j]=0;
			}
		}
	}

	xpos=0;
	nsamples=0;

	maxdata = new lsampl_t[nComediDevices];
	crange = new comedi_range*[nComediDevices];
	for(int devNo=0;devNo<nComediDevices;devNo++) {
		maxdata[devNo]=comedi_get_maxdata(dev[devNo],subdevice,0);
		crange[devNo]=comedi_get_range(dev[devNo],subdevice,0,0);
	}

	iirnotch = new Iir::Butterworth::BandStop<IIRORDER>**[nComediDevices];
	adAvgBuffer = new float*[nComediDevices];
	daqData = new lsampl_t*[nComediDevices];
	for(int devNo=0;devNo<nComediDevices;devNo++) {
		iirnotch[devNo] = new Iir::Butterworth::BandStop<IIRORDER>*[channels_in_use];
		adAvgBuffer[devNo]=new float[channels_in_use];
		for(int i=0;i<channels_in_use;i++) {
			adAvgBuffer[devNo][i]=0;
			iirnotch[devNo][i] = new Iir::Butterworth::BandStop<IIRORDER>;
		}
		daqData[devNo] = new lsampl_t[channels_in_use];
		for(int i=0;i<channels_in_use;i++) {
			daqData[devNo][i]=0;
		}
	}

	setNotchFrequency(f);

	counter = new QTimer( this );
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
	if (hdf5valid) {
		H5PTclose( hdf5table_id );
		H5PTclose( hdf5file_id );
		H5Tclose( hdf5compoundtype );
	}
}


void ComediScope::setNotchFrequency(float f) {
	for(int j=0;j<nComediDevices;j++) {
		for(int i=0;i<channels_in_use;i++) {
			float frequency_width = f/10;
			iirnotch[j][i]->setup (IIRORDER, 
					       sampling_rate, 
					       f, 
					       frequency_width);
		}
		notchFrequency = f;
	}
}



void ComediScope::updateTime() {
	QString s;
	if ((!rec_file)&&(!hdf5valid)) {
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
			
			sprintf(tmp,"%+.4fV",phys);
			comediRecord->voltageTextEdit[n][i]->setText(tmp);
		}
	}
}


int ComediScope::setFilename(QString name,int csv,int hdf5) {
	(*rec_filename)=name;
	recorded=0;
	recordInHDF5=hdf5;
	if (recordInHDF5) return 0;
	if (csv) {
		separator=',';
	} else {
		separator=' ';
	}
	return 0;
}


void ComediScope::writeFile() {
	float t = ((float)nsamples)/((float)sampling_rate);
	if (comediRecord->
	    rawCheckbox->isChecked()) {
		if (rec_file) fprintf(rec_file,"%ld",nsamples);
		if (hdf5floatBuffer) hdf5floatBuffer[0] = t;
	} else {
		if (rec_file) fprintf(rec_file,"%f",t);
		if (hdf5floatBuffer) hdf5floatBuffer[0] = t;
	}
	int chIdx = 1;
	for(int n=0;n<nComediDevices;n++) {
		for(int i=0;i<channels_in_use;i++) {
			if (comediRecord->
			    channelCheckbox[n][i]->isChecked()
				) {
				if (comediRecord->
				    rawCheckbox->isChecked()) {
					if (rec_file) 
						fprintf(rec_file,
							"%c%d",
							separator,
							(int)(daqData[n][i]));
					if (hdf5floatBuffer) 
						hdf5floatBuffer[chIdx++] = daqData[n][i];
				} else {
					float phy=comedi_to_phys((lsampl_t)(daqData[n][i]),
								 crange[n],
								 maxdata[n]);
					if (rec_file) 
						fprintf(rec_file,"%c%f",separator,phy);
					if (hdf5floatBuffer)
						hdf5floatBuffer[chIdx++] = phy;
				}
			}
		}
	}
	if (ext_data_receive) {
		if (rec_file) 
			fprintf(rec_file,
				"%c%s",
				separator,
				ext_data_receive->getData());
	}
	if (rec_file) fprintf(rec_file,"\n");
	if ((hdf5valid)&&(recordInHDF5)&&(hdf5floatBuffer)) {
		H5PTappend( hdf5table_id, 1, hdf5floatBuffer );
	}
}

void ComediScope::startRec() {
	if (recorded) return;
	comediRecord->disableControls();
	nsamples=0;
	if (recordInHDF5) {
		// create the buffer
		if (hdf5floatBuffer) delete[] hdf5floatBuffer;
		hdf5floatBuffer = new float[num_channels+1];
		
		// create the compound
		hdf5compoundtype = H5Tcreate (H5T_COMPOUND,
					      sizeof(float)*(num_channels+1)
			);
		
		herr_t status = H5Tinsert (hdf5compoundtype,
					   "t/sec",
					   0, 
					   H5T_NATIVE_FLOAT);

		int idxNo = 1;
		for(int n=0;n<nComediDevices;n++) {
			for(int i=0;i<channels_in_use;i++) {
				if (comediRecord->
				    channelCheckbox[n][i]->isChecked()) {	
					char tmp[256];
					if (comediRecord->
					    rawCheckbox->isChecked()) {
						sprintf(tmp,
							"dev%dch%d[raw]",n,i);	
					} else {
						sprintf(tmp,
							"dev%dch%d[u/V]",n,i);
					}
					status = H5Tinsert (hdf5compoundtype, 
							    tmp, 
							    sizeof(float)*idxNo,
							    H5T_NATIVE_FLOAT);
					idxNo++;
				}
			}
		}

		hdf5file_id = H5Fcreate (
			rec_filename->toLocal8Bit().constData(), 
			H5F_ACC_EXCL,
			H5P_DEFAULT, H5P_DEFAULT);
		
		hdf5table_id= H5PTcreate_fl(
			hdf5file_id,
			"/comedirecord", 
			hdf5compoundtype, 
			500,
			0);
		
		hdf5valid = 1;
	} else {
		rec_file=fopen(rec_filename->toLocal8Bit().constData(),
			       "wt");
	}
}


void ComediScope::stopRec() {
	if (rec_file) {
		fclose(rec_file);
		rec_file=NULL;
		recorded=1;
	}
	if (hdf5valid) {
		if (hdf5floatBuffer) {
			delete[] hdf5floatBuffer;
			hdf5floatBuffer = NULL;
		}
		H5PTclose( hdf5table_id );
		H5PTclose( hdf5file_id );
		H5Tclose( hdf5compoundtype );
		hdf5valid = 0;
	}
	comediRecord->enableControls();
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
	num_channels=0;

	for(int n=0;n<nComediDevices;n++) {
		for(int i=0;i<channels_in_use;i++) {
			if (comediRecord->channelCheckbox[n][i]->isChecked()) {
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
	int act=0;
	for(int n=0;n<nComediDevices;n++) {
		float dy=(float)base/(float)maxdata[n];
		for(int i=0;i<channels_in_use;i++) {
			if (comediRecord->
			    channelCheckbox[n][i]->
			    isChecked()) {
				paint.setPen(penData[act%3]);
				int yZero=base/2+base*act;
				float gain=comediRecord->gain[n][i]->getGain();
				float value = buffer[n][i] * gain;
				int yTmp=yZero-(int)(value*dy);
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
				float value;
				if(subdev_flags & SDF_LSAMPL) {
					value= ((float)((lsampl_t *)buffer)[i]);
				} else {
					value= ((float)((sampl_t *)buffer)[i]);
				}
				daqData[n][i] = value;
				value=(float)(value-(maxdata[n]/2));
				value = comediRecord->dcSub[n][i]->filter(value);
				if (comediRecord->filterCheckbox->checkState()==Qt::Checked) {
					value=iirnotch[n][i]->filter(value);
				}
				adAvgBuffer[n][i] = adAvgBuffer[n][i] + value;
			}
		}

		if (comediRecord->recPushButton->checkState()==Qt::Checked) {
			writeFile();
		}

		nsamples++;
		tb_counter--;

		if (tb_counter<=0) {
			for(int n=0;n<nComediDevices;n++) {
				for(int i=0;i<channels_in_use;i++) {
					adAvgBuffer[n][i]=adAvgBuffer[n][i]/tb_init;
				}
			}
		
			paintData(adAvgBuffer);

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
	xpos=0;
	repaint();
}
