#include "lp.h"

#include<qfont.h>
#include<stdio.h>

Lp::Lp(float samplingrate,float cutoff) : QCheckBox() {
	lp.setup (samplingrate, 
		  cutoff);
}

float Lp::filter (float v) {
	if (checkState()==Qt::Checked) {
		return lp.filter(v);
	} else {
		return v;
	}
}
