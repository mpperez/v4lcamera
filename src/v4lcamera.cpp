/*
 * v4lcamera.cpp: Linux v4l camera adquisition library/ program viewer Qt5.
 *
 * (C) Copyright 2015 mpperez
 * Author: mperez
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
/*!\file
	\brief Linux v4l camera adquisition library/ program viewer.
	\date 05-2015
	\author mperez

*/
#include "v4lcamera.h"
#include "ui_v4lcamera.h"
#include "qscrollbar.h"
int v4lcamera::m_ncams=0;
pthread_t v4lcamera::m_pthreadhiloadq[MAX_CAMS];
v4lcamera::v4lcamera(QWidget *parent,bool workaslibrary/*=1*/) :
	QMainWindow(parent),
	ui(new Ui::v4lcamera)
{
	m_stop=0;
	m_islibrary=workaslibrary;
	if(m_ncams==0)
		memset(m_pthreadhiloadq,0,sizeof(m_pthreadhiloadq));
	if(!m_islibrary)
		pthread_create(&m_pthreadhiloadq[m_ncams],NULL,ThreadAdq,this);
	m_curcam=m_ncams;
	m_ncams++;
	ui->setupUi(this);
	UpdateComboDevices();
	//UpdateDevicesMap();
	this->setVisible(1);
	m_sigmappercombos=NULL;
	m_sigmappercchecbx=NULL;
	m_sigmappersliders=NULL;
	m_sigmappersspinbox=NULL;
	if(!m_islibrary)
	{
		m_visor=new QWidget(NULL);
		m_visor->resize(480,320);
		m_layoutvisor=new QGridLayout(m_visor);
		m_layoutvisor->setMargin(0);
		m_framevisor.setScaledContents(1);
		m_layoutvisor->addWidget(&m_framevisor,0,0);
		m_visor->show();
		connect(this, SIGNAL(SGNewImage(v4l2image *)), this, SLOT(OnNewImage(v4l2image *)),Qt::QueuedConnection);
		connect(&m_timer1s, SIGNAL(timeout()), this, SLOT(OnTimer1s()));
		m_timer1s.start(1000);
	}
//	on_pushButton_open_device_clicked();
}
v4lcamera::~v4lcamera()
{
	delete ui;
	int ms=2;
	m_stop=1;
	struct timespec ts = { ms / 1000, (ms % 1000) * 1000 * 1000 };
	for(int i=1;i<20;i++)
	{
		nanosleep(&ts, NULL);
		if(m_stop==-1)
			break;
	}

}
void v4lcamera::UpdateComboDevices()
{
	int err=UpdateDevicesMap();
	if(!err)
	{
		ui->comboBox_device_list->clear();
		int cont=0;
		for(itermapvl4capdevice iter=m_devicesmap.begin();iter!=m_devicesmap.end();iter++,cont++)
			ui->comboBox_device_list->insertItem(cont, QString::fromStdString(iter->first));
	}
	else
	{
		ui->comboBox_device_list->clear();
		ui->comboBox_device_list->addItem("No devices found");
	}
}
void v4lcamera::UpdateComboStandards()
{
	if(m_status==V4LCAP_CLOSED)
		ui->comboBox_standaard_list->clear();
	int count=0;
	string tx;
	int width,height,fps;
	if(0!=GetCurrentResolution(&tx,&width,&height,&fps))
		AddMessage(GetLastErrorstr());
	int current=-1;
	for(itermapresolution iter=m_resolutions.begin();iter!=m_resolutions.end();iter++,count++)
	{
		ui->comboBox_standaard_list->insertItem(count,iter->first.data());
		if(tx==iter->first)
			current=count;
	}
	ui->comboBox_standaard_list->setCurrentIndex(current);
	UpdateComboResolutions();
}
void v4lcamera::UpdateComboResolutions()
{
	if(m_status==V4LCAP_CLOSED)
		ui->comboBox_resolution_list->clear();
	if(m_resolutions.size()==0)
		return;
	itermapresolution resit=m_resolutions.find(ui->comboBox_standaard_list->currentText().toLatin1().data());
	if(resit==m_resolutions.end())
		return;
	int count =0;
	string tx;
	int width,height,fps;
	if(0!=GetCurrentResolution(&tx,&width,&height,&fps))
		AddMessage(GetLastErrorstr());
	int current=-1;
	vector <resolutionstr> res=resit->second;
	for(vector <resolutionstr>::iterator iter=res.begin();iter!=res.end();iter++,count++)
	{
		resolutionstr val=*iter;
		QString txt;
		if(val.type==V4L2_FRMSIZE_TYPE_DISCRETE)
		{
			txt=QString::number(val.discreteresolution.width)+ "x"+QString::number(val.discreteresolution.height);
			if(width==(int)val.discreteresolution.width && height==(int)val.discreteresolution.height )
				current=count;
		}
		else
		{
			txt=QString::number(val.stepwiseresolution.min_width)+ "x"+QString::number(val.stepwiseresolution.min_height);
			txt+=" to "+QString::number(val.stepwiseresolution.max_width)+ "x"+QString::number(val.stepwiseresolution.max_height);
			current=count;
		}
		for(vector <frameintervalsstr>::iterator iterf=val.frameintervals.begin();iterf!=val.frameintervals.end();iterf++)
		{
			frameintervalsstr frm=*iterf;
			txt+="  ";
			if(frm.frameratetype==V4L2_FRMIVAL_TYPE_DISCRETE)
			{
				txt+=QString::number(frm.discretefps.denominator)+"hz";
			}
			else
				txt+=QString::number(frm.stepwisefps.min.denominator)+" to "+ QString::number(frm.stepwisefps.max.denominator)+"hz";
		}
		ui->comboBox_resolution_list->insertItem(count,txt);
	}
	ui->comboBox_resolution_list->setCurrentIndex(current);
}
void v4lcamera::CreateControls()
{
	if(m_status==V4LCAP_CLOSED)
		ui->comboBox_resolution_list->clear();
	if(m_resolutions.size()==0)
		return;//nothing to do here
	ui->gridLayout->setSizeConstraint(QLayout::SetMinimumSize);
	int cont=0;
	ClearControls();
	m_sigmappercombos=new QSignalMapper;
	m_sigmappercchecbx=new QSignalMapper;
	m_sigmappersliders=new QSignalMapper;
	m_sigmappersspinbox=new QSignalMapper;
	for (itermapcontrolstr iter=m_controls.begin();iter!=m_controls.end();iter++,cont++)
	{
		controlstr val=iter->second;
		if (val.type==V4L2_CTRL_TYPE_MENU || val.type==V4L2_CTRL_TYPE_INTEGER_MENU)
			AddControlMenu(iter->first,val,cont);
		if (val.type==V4L2_CTRL_TYPE_INTEGER || val.type==V4L2_CTRL_TYPE_INTEGER64)
			AddControlInt(iter->first,val,cont);
		if (val.type==V4L2_CTRL_TYPE_BOOLEAN)
			AddControlCheckbox(iter->first,val,cont);
		//ui->gridLayout->setRowMinimumHeight(cont,40);
	}
	connect(m_sigmappercombos, SIGNAL(mapped(const QString &)),
							this, SLOT(AplyConfigCombo(QString)));
	connect(m_sigmappercchecbx, SIGNAL(mapped(const QString &)),
							this, SLOT(AplyConfigCheckbox(QString)));
	connect(m_sigmappersliders, SIGNAL(mapped(const QString &)),
							this, SLOT(AplyConfigSlider(QString)));
	connect(m_sigmappersspinbox, SIGNAL(mapped(const QString &)),
							this, SLOT(AplyConfigDSpinbox(QString)));
	//UpdateDialogValues();
}
void v4lcamera::AddControlMenu(string name,controlstr val,int pos)
{
	m_controlmutex.lock();
	QLabel *lab=new QLabel(QString::fromStdString(name),ui->scrollArea);
	ui->gridLayout->addWidget(lab,pos,0);
	lab->show();
	QComboBox *cbox=new QComboBox();
	int cont=0;
	for(vector <string>::iterator iter=val.data.begin();iter!=val.data.end();iter++,cont++)
	{
		cbox->insertItem(cont,QString::fromStdString(*iter));
	}
	cbox->show();
	cbox->setEnabled(!IS_CONTROL_DISABLED(val.flags));
	int cur;
	if(0==GetControlValue(name,&cur))
		cbox->setCurrentIndex(cur);
	else
		cbox->setCurrentIndex(val.defvalue);
	ui->gridLayout->addWidget(cbox,pos,1);
	pair <QString,QComboBox*> dat(QString::fromStdString(name),cbox);
	m_vectcontrolcombos.append(dat);
	connect(cbox, SIGNAL(currentIndexChanged(int)), m_sigmappercombos, SLOT(map()));
	m_sigmappercombos->setMapping(cbox, QString::fromStdString(name));
	m_vectcontrollabels.append(lab);
	m_controlmutex.unlock();
}
void v4lcamera::AddControlInt(string name,controlstr val,int pos)
{
	m_controlmutex.lock();
	QLabel *lab=new QLabel(QString::fromStdString(name),ui->scrollArea);
	ui->gridLayout->addWidget(lab,pos,0);
	lab->show();
	QSlider *sli=new QSlider(Qt::Horizontal);
	sli->setEnabled(!IS_CONTROL_DISABLED(val.flags));
	sli->show();
	sli->setMaximum(val.max);
	sli->setMinimum(val.min);
	sli->setSingleStep(val.step);
	int cur;
	if(0==GetControlValue(name,&cur))
		sli->setValue(cur);
	else
		sli->setValue(val.defvalue);
	ui->gridLayout->addWidget(sli,pos,1);
	QDoubleSpinBox * spin=new QDoubleSpinBox();
	spin->setDecimals(0);
	spin->setEnabled(!IS_CONTROL_DISABLED(val.flags));
	spin->setMaximum(val.max);
	spin->setMinimum(val.min);
	spin->setSingleStep(val.step);
	if(0==GetControlValue(name,&cur))
		spin->setValue(cur);
	else
		spin->setValue(val.defvalue);
	ui->gridLayout->addWidget(spin,pos,2);
	pair <QString,QDoubleSpinBox*> dat(QString::fromStdString(name),spin);
	m_vectcontroldspinbox.append(dat);
	pair <QString,QSlider*> dat1(QString::fromStdString(name),sli);
	m_vectcontrolsliders.append(dat1);
	m_vectcontrollabels.append( lab);
	connect(spin, SIGNAL(valueChanged(double)), m_sigmappersspinbox, SLOT(map()));
	m_sigmappersspinbox->setMapping(spin, QString::fromStdString(name));
	connect(sli, SIGNAL(valueChanged(int)), m_sigmappersliders, SLOT(map()));
	m_sigmappersliders->setMapping(sli, QString::fromStdString(name));
	m_controlmutex.unlock();
}
void v4lcamera::AddControlCheckbox(string name,controlstr val,int pos)
{
	m_controlmutex.lock();
	QLabel *lab=new QLabel(QString::fromStdString(name));
	ui->gridLayout->addWidget(lab,pos,0);
	lab->show();
	QCheckBox *chk=new QCheckBox();
	chk->show();
	chk->setEnabled(!IS_CONTROL_DISABLED(val.flags));
	int cur;
	if(0==GetControlValue(name,&cur))
		chk->setChecked(cur);
	else
		chk->setChecked(val.defvalue);
	ui->gridLayout->addWidget(chk,pos,1);
	pair <QString,QCheckBox*> dat(QString::fromStdString(name),chk);
	m_vectcontrolcheckbox.append(dat);
	connect(chk, SIGNAL(clicked()), m_sigmappercchecbx, SLOT(map()));
	m_sigmappercchecbx->setMapping(chk, QString::fromStdString(name));
	m_vectcontrollabels.append( lab);
	m_controlmutex.unlock();
}
void v4lcamera::ClearControls()
{
	m_controlmutex.lock();
	if(NULL!=m_sigmappersliders)
			delete m_sigmappersliders;
	if(NULL!=m_sigmappercombos)
			delete m_sigmappercombos;
	if(NULL!=m_sigmappercchecbx)
			delete m_sigmappercchecbx;
	if(NULL!=m_sigmappersspinbox)
			delete m_sigmappersspinbox;
	m_sigmappercombos=NULL;
	m_sigmappercchecbx=NULL;
	m_sigmappersliders=NULL;
	m_sigmappersspinbox=NULL;
	for(QVector < pair<QString,QComboBox*> >::iterator iter=m_vectcontrolcombos.begin();iter!=m_vectcontrolcombos.end();iter++)
	{
		if(iter->second!=NULL)
		{
			ui->gridLayout->removeWidget(iter->second);
			delete iter->second;
		}
	}
	m_vectcontrolcombos.clear();
	for(QVector <QLabel>::iterator *iter=m_vectcontrollabels.begin();iter!=m_vectcontrollabels.end();iter++)
	{
		if(*iter!=NULL)
		{
			ui->gridLayout->removeWidget(*iter);
			delete *iter;
		}
	}
	m_vectcontrollabels.clear();
	for(QVector <pair<QString,QDoubleSpinBox*> >::iterator iter=m_vectcontroldspinbox.begin();iter!=m_vectcontroldspinbox.end();iter++)
	{
		if(iter->second!=NULL)
		{
			ui->gridLayout->removeWidget(iter->second);
			delete iter->second;
		}
	}
	m_vectcontroldspinbox.clear();
	for(QVector <pair<QString,QSlider*> >::iterator iter=m_vectcontrolsliders.begin();iter!=m_vectcontrolsliders.end();iter++)
	{
		if(iter->second!=NULL)
		{
			ui->gridLayout->removeWidget(iter->second);
			delete iter->second;
		}
	}
	m_vectcontrolsliders.clear();
	for(QVector <pair<QString,QCheckBox*> >::iterator iter=m_vectcontrolcheckbox.begin();iter!=m_vectcontrolcheckbox.end();iter++)
	{
		if(iter->second!=NULL)
		{
			ui->gridLayout->removeWidget(iter->second);
			delete iter->second;
		}
	}
	m_vectcontrolcheckbox.clear();
	m_controlmutex.unlock();
}
void v4lcamera::UpdateDialogValues()
{
	int val;
	for(QVector < pair<QString,QCheckBox*> >::iterator iter=m_vectcontrolcheckbox.begin();iter!=m_vectcontrolcheckbox.end();iter++)
	{
			QCheckBox*cb=(QCheckBox*)iter->second;
			cb->setEnabled(IsControlEnabled(iter->first.toStdString()));
			if(0==GetControlValue(iter->first.toStdString(),&val))
				cb->setChecked(val);
			else
				AddMessage(GetLastErrorstr());
	}
	for(QVector <pair<QString,QSlider*> >::iterator iter=m_vectcontrolsliders.begin();iter!=m_vectcontrolsliders.end();iter++)
	{
		int val=IsControlEnabled(iter->first.toStdString());
		((QSlider*)iter->second)->setEnabled(val);
		if(0==GetControlValue(iter->first.toStdString(),&val))
			((QSlider*)iter->second)->setValue(val);
		else
			AddMessage(GetLastErrorstr());
	}
	for(QVector <pair<QString,QDoubleSpinBox*> >::iterator iter=m_vectcontroldspinbox.begin();iter!=m_vectcontroldspinbox.end();iter++)
	{
		int val=IsControlEnabled(iter->first.toStdString());
		((QDoubleSpinBox*)iter->second)->setEnabled(val);
		if(0==GetControlValue(iter->first.toStdString(),&val))
			((QDoubleSpinBox*)iter->second)->setValue(val);
		else
			AddMessage(GetLastErrorstr());
	}
}
void v4lcamera::UpdateVisivilityControls()
{
	for(QVector <pair<QString,QSlider*> >::iterator iter=m_vectcontrolsliders.begin();iter!=m_vectcontrolsliders.end();iter++)
	{
		int val=IsControlEnabled(iter->first.toStdString());
		((QSlider*)iter->second)->setEnabled(val);
	}
	for(QVector <pair<QString,QDoubleSpinBox*> >::iterator iter=m_vectcontroldspinbox.begin();iter!=m_vectcontroldspinbox.end();iter++)
	{
		int val=IsControlEnabled(iter->first.toStdString());
		((QDoubleSpinBox*)iter->second)->setEnabled(val);
	}

}
//
//working as application
//
void v4lcamera::UpdateImage(v4l2image *img)
{
//m_visor->setWindowTitle(QString::number(m_realfps,'f',1)+"Hz");
	if(img==NULL)
		return;
	if(img->bytespixel==0)
		return;
	string pxf=img->pixformat;
	pxf.erase(pxf.begin()+4,pxf.end());
	if(pxf==string("YUYV"))
	{
		unsigned char*dt=new unsigned char[img->width*(img->height)*3];
		YUV422toRGB888(img->width, img->height,(unsigned char*)img->pointer,dt);
		QImage data(dt,img->width, img->height,3*img->width,QImage::Format_RGB888);
		//	ui->labelbackimage->setPixmap(QPixmap::fromImage(data));
		m_framevisor.setPixmap(QPixmap::fromImage(data));
		delete []dt;
	}
	if(pxf=="RGB3")
	{
		QImage data((uchar*)img->pointer, img->width, img->height, img->bytesline,QImage::Format_RGB888);
		m_framevisor.setPixmap(QPixmap::fromImage(data));
	}
	m_framevisor.setMinimumHeight(10);
	m_framevisor.setMinimumWidth(10);
	//delete img;
	this->update();
}
void v4lcamera::YUV422toRGB888(int width, int height, unsigned char *src, unsigned char *dst)
{
 int line, column;
 unsigned char *py, *pu, *pv;
 unsigned char *tmp = dst;

 /* In this format each four bytes is two pixels. Each four bytes is two Y's, a Cb and a Cr.
		Each Y goes to one of the pixels, and the Cb and Cr belong to both pixels. */
 py = src;
 pu = src + 1;
 pv = src + 3;

 #define CLIP(x) ( (x)>=0xFF ? 0xFF : ( (x) <= 0x00 ? 0x00 : (x) ) )

 for (line = 0; line < height; ++line) {
	 for (column = 0; column < width; ++column) {
		 *tmp++ = CLIP((double)*py + 1.402*((double)*pv-128.0));
		 *tmp++ = CLIP((double)*py - 0.344*((double)*pu-128.0) - 0.714*((double)*pv-128.0));
		 *tmp++ = CLIP((double)*py + 1.772*((double)*pu-128.0));

		 // increase py every time
		 py += 2;
		 // increase pu,pv every second time
		 if ((column & 1)==1) {
			 pu += 4;
			 pv += 4;
		 }
	 }
 }
}
void v4lcamera::AddMessage(string msg)
{
	ui->statusbar->showMessage(QString::fromStdString(msg));
}
//
//working as library(external control)
//
QVector<QString> v4lcamera::GetDeviceList()
{
	QVector<QString> current;
	for(int r=0;r<ui->comboBox_device_list->count();r++)
		current.append(ui->comboBox_device_list->itemText(r));
	return current;
}
int v4lcamera::OpenDevice(QString devicename)
{
	int index = ui->comboBox_device_list->findText(devicename);
	if(index!=-1 || m_status!=V4LCAP_CLOSED)
		ui->comboBox_device_list->setCurrentIndex(index);
	else
		return -1;
	if(!OpenDeviceName(devicename.toLatin1().data()))
	{
		UpdateComboStandards();
		CreateControls();
		ui->pushButton_open_device->setText("Close");
		//if(!m_islibrary)
			ui->pushButton_start->setEnabled(1);
		ui->pushButton_set_resolution->setEnabled(1);
		ui->comboBox_resolution_list->setEnabled(1);
		ui->comboBox_standaard_list->setEnabled(1);
	}
	else
		AddMessage(GetLastErrorstr());
	return 0;
}
int v4lcamera::CloseCurrentDevice( )
{
	if(0!=CloseDevice())
	{
		AddMessage(GetLastErrorstr());
		return -1;
	}
	ClearControls();
	ui->pushButton_start->setEnabled(0);
	ui->pushButton_open_device->setText("Open");
	ui->pushButton_set_resolution->setEnabled(0);
	UpdateComboStandards();
	ui->pushButton_start->setText("Start");
	return 0;
}

QVector<QString> v4lcamera::GetStandardsList()
{
	QVector<QString> current;
	for(int r=0;r<ui->comboBox_standaard_list->count();r++)
		current.append(ui->comboBox_standaard_list->itemText(r));
	return current;
}
int v4lcamera::SetStandard(QString standardname)
{
	int index = ui->comboBox_standaard_list->findText(standardname);
	if(index!=-1)
		ui->comboBox_standaard_list->setCurrentIndex(index);
	else
		return -1;
	int width,height;
	QString resolutionname=ui->comboBox_resolution_list->currentText();
	QStringList list = resolutionname.split("x",QString::SkipEmptyParts);
	if(list.size()==2)
	{
		width=list.at(0).toInt();
		QStringList list1 = list.at(1).split(" ");
		if(list1.size()>=2)
			height=list1.at(0).toInt();
		if(0==v4lcapture::SetResolution(standardname.toStdString(),width,height))
			AddMessage("Resolution changed");
		if(0!=InitCaptureBuffers(4))
			AddMessage(GetLastErrorstr());
	}
	else
		AddMessage(GetLastErrorstr());
	return 0;
	return 0;
}
QVector<QString> v4lcamera::GetResolutionsList()
{
	QVector<QString> current;
	for(int r=0;r<ui->comboBox_resolution_list->count();r++)
		current.append(ui->comboBox_resolution_list->itemText(r));
	return current;
}
int v4lcamera::SetResolution(QString resolutionname)
{
	int index = ui->comboBox_resolution_list->findText(resolutionname);
	if(index!=-1)
		ui->comboBox_resolution_list->setCurrentIndex(index);
	else
		return -1;
	int width,height;
	QStringList list = resolutionname.split("x",QString::SkipEmptyParts);
	if(list.size()==2)
	{
		width=list.at(0).toInt();
		QStringList list1 = list.at(1).split(" ");
		if(list1.size()>=2)
			height=list1.at(0).toInt();
		if(0==v4lcapture::SetResolution(ui->comboBox_standaard_list->currentText().toStdString(),width,height))
			AddMessage("Resolution changed");
		if(0!=InitCaptureBuffers(4))
			AddMessage(GetLastErrorstr());
	}
	else
		AddMessage(GetLastErrorstr());
	return 0;
}
QVector<QString> v4lcamera::GetBoolParametersName()
{
	QVector<QString> current;
	for(QVector < pair<QString,QCheckBox*> >::iterator iter=m_vectcontrolcheckbox.begin();iter!=m_vectcontrolcheckbox.end();iter++)
		current.append(iter->first);
	return current;
}
int v4lcamera::GetBoolParametersName(QString parname,bool *current)
{
	for(QVector < pair<QString,QCheckBox*> >::iterator iter=m_vectcontrolcheckbox.begin();iter!=m_vectcontrolcheckbox.end();iter++)
	{
		if(iter->first==parname)
		{
			*current=iter->second->isChecked();
			return 0;
		}
	}
	return 1;
}
int v4lcamera::SetBoolParametersName(QString parname,bool value)
{
	for(QVector < pair<QString,QCheckBox*> >::iterator iter=m_vectcontrolcheckbox.begin();iter!=m_vectcontrolcheckbox.end();iter++)
	{
		if(iter->first==parname)
		{
			iter->second->setChecked(value);
			return 0;
		}
	}
	return -1;
}
QVector<QString> v4lcamera::GetAnalogsParametersName()
{
	QVector<QString> current;
	for(QVector <pair<QString,QSlider*> >::iterator iter=m_vectcontrolsliders.begin();iter!=m_vectcontrolsliders.end();iter++)
		current.append(iter->first);
	return current;
}
int v4lcamera::GetAnalogParameter(QString parname,float *current,float *minvalue,float *maxvalue)
{
	for(QVector <pair<QString,QSlider*> >::iterator iter=m_vectcontrolsliders.begin();iter!=m_vectcontrolsliders.end();iter++)
	{
		if(iter->first==parname)
		{
			if(current!=NULL)
				*current=iter->second->value();
			if(minvalue!=NULL)
				*minvalue=iter->second->minimum();
			if(maxvalue!=NULL)
				*maxvalue=iter->second->maximum();
			return 0;
		}
	}
	return -1;
}
int v4lcamera::SetAnalogParameter(QString parname,float value)
{
	for(QVector <pair<QString,QSlider*> >::iterator iter=m_vectcontrolsliders.begin();iter!=m_vectcontrolsliders.end();iter++)
	{
		if(iter->first==parname)
		{
			iter->second->setValue(value);
			return 0;
		}
	}
	return -1;
}
QVector<QString> v4lcamera::GetSelectableParametersName()
{
	QVector<QString> current;
	for(QVector < pair<QString,QComboBox*> >::iterator iter=m_vectcontrolcombos.begin();iter!=m_vectcontrolcombos.end();iter++)
		current.append(iter->first);
	return current;
}
int v4lcamera::GetSelectableParameter(QString parname,int *current)
{
	for(QVector < pair<QString,QComboBox*> >::iterator iter=m_vectcontrolcombos.begin();iter!=m_vectcontrolcombos.end();iter++)
	{
		if(iter->first==parname)
		{
			if(current!=NULL)
				*current=iter->second->currentIndex();
			return 0;
		}
	}
	return -1;
}
int v4lcamera::SetSelectableParameter(QString parname,int value)
{
	for(QVector < pair<QString,QComboBox*> >::iterator iter=m_vectcontrolcombos.begin();iter!=m_vectcontrolcombos.end();iter++)
	{
		if(iter->first==parname)
		{
			iter->second->setCurrentIndex(value);
			return 0;
		}
	}
	return -1;
}
int v4lcamera::Start()
{
	if(0==StartAdquisition())
	{
		ui->pushButton_start->setText("Stop");
		ui->pushButton_set_resolution->setEnabled(0);
		ui->comboBox_resolution_list->setEnabled(0);
		ui->comboBox_standaard_list->setEnabled(0);
		return 0;
	}
	else
		AddMessage(GetLastErrorstr());
	return -1;
}
int v4lcamera::Stop()
{
	if(0==StopAdquisition())
	{
		ui->pushButton_start->setText("Start");
		ui->pushButton_set_resolution->setEnabled(1);
		ui->comboBox_resolution_list->setEnabled(1);
		ui->comboBox_standaard_list->setEnabled(1);
		return 0;
	}
	else
		AddMessage(GetLastErrorstr());
	return -1;
}
//
//TIMERS
//
void v4lcamera::OnTimer1s()
{
		m_visor->setWindowTitle(QString::number(m_realfps,'f',1)+"Hz");
}
//
//DIALOG
//
void v4lcamera::on_pushButton_open_device_clicked()
{
	if(m_status==V4LCAP_CLOSED)
		OpenDevice(ui->comboBox_device_list->currentText());
	else
		this->CloseCurrentDevice();
}
void v4lcamera::on_pushButton_update_devicelist_clicked()
{
	UpdateComboDevices();
	AddMessage(GetLastErrorstr());
}
void v4lcamera::on_comboBox_device_list_currentIndexChanged(int )
{

}
void v4lcamera::on_pushButton_set_resolution_clicked()
{
	SetResolution(ui->comboBox_resolution_list->currentText());//change resolution and standard
}
void v4lcamera::on_pushButton_start_clicked()
{
	if(ui->pushButton_start->text()=="Start")
		Start();
	else
		Stop();
}
//
//FROM DYNAMIC CONROLS
//
int v4lcamera::AplyConfigCombo(QString controlname)
{
	for(QVector < pair<QString,QComboBox*> >::iterator iter=m_vectcontrolcombos.begin();iter!=m_vectcontrolcombos.end();iter++)
	{
		if(iter->first==controlname)
		{
			QComboBox*cb=iter->second;
			if(0!=SetControlValue(controlname.toStdString(),cb->currentIndex()))
				AddMessage(GetLastErrorstr());
		}
	}
	UpdateVisivilityControls();
	return 0;
}
int v4lcamera::AplyConfigDSpinbox(QString controlname)
{
	for(QVector <pair<QString,QDoubleSpinBox*> >::iterator iter=m_vectcontroldspinbox.begin();iter!=m_vectcontroldspinbox.end();iter++)
	{
		if(iter->first==controlname)
		{
			QDoubleSpinBox*cb=iter->second;
			if(0!=SetControlValue(controlname.toStdString(),cb->value()))
				AddMessage(GetLastErrorstr());
			for(QVector <pair<QString,QSlider*> >::iterator iter=m_vectcontrolsliders.begin();iter!=m_vectcontrolsliders.end();iter++)
			{//update slider
				if(iter->first==controlname)
				{
					QSlider*sld=iter->second;
					if(sld->value()!=cb->value())
						sld->setValue((double)cb->value());
				}
			}
		}
	}
	return 0;
}
int v4lcamera::AplyConfigSlider(QString controlname)
{
	for(QVector <pair<QString,QSlider*> >::iterator iter=m_vectcontrolsliders.begin();iter!=m_vectcontrolsliders.end();iter++)
	{
		if(iter->first==controlname)
		{
			QSlider*cb=iter->second;
			if(0!=SetControlValue(controlname.toStdString(),cb->value()))
				AddMessage(GetLastErrorstr());
			for(QVector <pair<QString,QDoubleSpinBox*> >::iterator iter=m_vectcontroldspinbox.begin();iter!=m_vectcontroldspinbox.end();iter++)
			{//update spinbox
				if(iter->first==controlname)
				{
					QDoubleSpinBox*spbx=iter->second;
					if(spbx->value()!=cb->value())
						spbx->setValue((double)cb->value());
				}
			}
		}
	}
	return 0;
}
int v4lcamera::AplyConfigCheckbox(QString controlname)
{
	for(QVector < pair<QString,QCheckBox*> >::iterator iter=m_vectcontrolcheckbox.begin();iter!=m_vectcontrolcheckbox.end();iter++)
	{
		if(iter->first==controlname)
		{
			QCheckBox*cb=(QCheckBox*)iter->second;
			if(0!=SetControlValue(controlname.toStdString(),cb->isChecked()))
				AddMessage(GetLastErrorstr());
		}
	}
	UpdateVisivilityControls();
	return 0;
}
//
//FROM ADQUISITION THREAD
//
int v4lcamera::OnNewImage(v4l2image *img)
{
	UpdateImage(img);
	delete img;
	return 0;
}
//
//ADQUISITION THTREAD
//
void* v4lcamera::ThreadAdq(void* param)
{
	v4lcamera* pthis=(v4lcamera*)param;
	char name[40];
	sprintf(name,"adq_cam_%d",pthis->m_curcam);
	pthread_setname_np(pthis->m_pthreadhiloadq[pthis->m_curcam],name);
	while(pthis->m_stop!=1)
	{
		int ms=20;
		struct timespec ts = { ms / 1000, (ms % 1000) * 1000 * 1000 };
		nanosleep(&ts, NULL);
		if(pthis->m_status==V4LCAP_RUN)
		{
			v4l2image *img;
			if(pthis->WaitNextFrame(&img,290)==0)//signal emit
				pthis->EmitNewImage(img);
		}
	}
	pthis->m_stop=-1;
	return NULL;
}
