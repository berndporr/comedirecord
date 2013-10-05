#include "fftscope.h"
#include <qtimer.h>
#include <qpainter.h>
#include <qapplication.h>
#include <qtimer.h>

#include<sys/ioctl.h>
#include<stdio.h>
#include<fcntl.h>
#include<unistd.h>
#include<stdlib.h>
#include<math.h>

#include <comedilib.h>
#include <fcntl.h>

#include <fftw3.h>
#include <qwt/qwt_plot.h>
#include <qwt/qwt_plot_curve.h>
#include "comedirecord.h"

FFTScope::FFTScope( ComediRecord *parent, int fft_buffer_size)
	: QMainWindow( parent, 0 )
{
	comedirecord=parent;
	setAttribute(Qt::WA_DeleteOnClose);
	setWindowFlags(Qt::Window);
	
	counter = new QTimer( this );
	connect( counter, 
		 SIGNAL(timeout()),
		 this, 
		 SLOT(updateFFT()) );
	counter->start( FFT_UPDATE_DELAY );

	buffer_size = fft_buffer_size;

	// fft init
	// real numbers in
	in  = (double*)fftw_malloc(sizeof(double) * buffer_size);
	int n_out = ((buffer_size/2)+1);
	// complex numbers out
	out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * n_out);
	current_entry = 0;
	filled = 0;

	// plotting
	fftPlot = new QwtPlot(this);
	fftPlot->setAxisAutoScale(QwtPlot::yLeft,true);
	fftPlot->setAxisAutoScale(QwtPlot::xBottom,true);
	QwtPlotCurve *fftCurve = new QwtPlotCurve("FFT");
	x = new double[buffer_size/2];
	y = new double[buffer_size/2];
	for(int i=0;i<buffer_size/2;i++) {
		x[i]=0.0;
		y[i]=0.0;
	}
	fftCurve->setRawSamples(x,y,buffer_size/2);
	fftCurve->attach(fftPlot);
	setCentralWidget(fftPlot);
}

FFTScope::~FFTScope()
{
	comedirecord->comediScope->fftch = -1;
	comedirecord->comediScope->fftdevno = -1;
	counter->stop();
	delete counter;
	fftw_free ( in  );
	fftw_free ( out );
	delete []x;
	delete []y;
}

void FFTScope::updateFFT()
{
	if (filled)
	{
		int i;

		fftw_plan plan_forward;

		// real in and complex out
		plan_forward = fftw_plan_dft_r2c_1d ( buffer_size, in, out, FFTW_ESTIMATE );

		// do it
		fftw_execute ( plan_forward );

		// plot the spectrum
		float yMax = 1E-10;
		for (i = 0; i < (buffer_size/2); i++)
		{
			x[i] =  ((double)i)*comedirecord->comediScope->sampling_rate/((double)buffer_size);
			double a = out[i][0];
			double b = out[i][1];
			y[i] = sqrt(a*a+b*b);
			if ((y[i]>yMax)&&(i>0)) yMax = y[i];
		}
		y[0]=0.0;

		fftPlot->setAxisScale(QwtPlot::yLeft,0,yMax);
		fftPlot->setAxisScale(QwtPlot::xBottom,0,comedirecord->comediScope->sampling_rate/2);
		fftw_destroy_plan ( plan_forward );
		fftPlot->replot();
		filled = 0;
		current_entry = 0;
	}
}

int FFTScope::append(double val)
{
	in[current_entry] = val ;
	current_entry++ ;
	if (current_entry == buffer_size)
	{
		current_entry = 0;
		filled = 1;
	}
	return filled;
}
