/*
 * v4lcapture.cpp: Class to interface with v4l camera adquisition library.Also store information into structures.
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
	\brief class to interface with v4l camera adquisition library.Also store information into structures.
	\date 05-2015
	\author mperez*/
#include "v4lcapture.h"
#include <errno.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <libv4l2.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <iostream>
#define VL4_DEVICES_DIR   "/sys/class/video4linux"

///STATIC VARS
mapvl4capdevice  v4lcapture::m_devicesmap;
QMutex v4lcapture::m_devicesmap_mutex;

v4lcapture::v4lcapture():
	m_capturesemaphore(1)
{
	m_fid=-1;
	m_status=V4LCAP_CLOSED;
	m_nbuffers=5;
	m_fpsclock.tv_sec=m_fpsclock.tv_usec=0;
}
v4lcapture::~v4lcapture()
{

}
int v4lcapture::OpenDevice(const char *path)
{
	//m_fid=open(path,O_RDWR);
	m_fid=v4l2_open(path, O_RDWR | O_NONBLOCK, 0);
	m_currdevicepath=path;
	if (m_fid < 0)
	{
		printf("Error opening device:%s",path);
		return -1;
	}
	m_status=V4LCAP_UNCONFIGURED;
	GetInfoDevice(path,&m_current_device);
	GetAvailableResolutions();
	GetCurrentResolution(NULL,NULL,NULL,NULL);
	//printf("%s\n",GetResolutionsInformation().data());
	GetCurrentControls();
//	printf("%s\n",	GetControlsInformation().data());
	return 0;
}
int v4lcapture::OpenDeviceName(char *name)
{
	itermapvl4capdevice it=m_devicesmap.find(name);
	if(it==m_devicesmap.end())
		return -1;
	v4lcapdevice  dv=m_devicesmap.at(name);
	return OpenDevice(dv.v4l2_name.data());
}
int v4lcapture::xioctl(int fh, int request, void *arg)
{
	int r;
	do
	{
		r = v4l2_ioctl(fh, request, arg);
	} while (r == -1 && ((errno == EINTR) || (errno == EAGAIN)));
	if (r == -1)
	{
			 return -1;
	}
	return 0;
}
int v4lcapture::CloseDevice()
{
	if(m_status==V4LCAP_RUN)
		StopAdquisition();
	m_resolutions.clear();
	ReleaseCaptureBuffers();
	if (m_fid >= 0)
	v4l2_close(m_fid);
	m_status=V4LCAP_CLOSED;
	m_fid=-1;
	return 0;
}
int v4lcapture::UpdateDevicesMap()
{
	m_devicesmap_mutex.lock();
	DIR *v4l_dir = opendir(VL4_DEVICES_DIR);
	if(v4l_dir==NULL)
	{
		m_devicesmap_mutex.unlock();
		return -1;
	}
	m_devicesmap.clear();
	struct dirent *dir_entry;
	while((dir_entry = readdir(v4l_dir)))
	{
		if(strstr(dir_entry->d_name, "video") != dir_entry->d_name &&		// search video device
				strstr(dir_entry->d_name, "subdev") != dir_entry->d_name)
			continue;
		string devi="/dev/";
		devi+=		dir_entry->d_name;
		v4lcapdevice v4ldev;
		GetInfoDevice(devi,&v4ldev );
		string devname=v4ldev.v4l2_name.data();//device name for map identifier
		v4ldev.v4l2_name="/dev/";
		v4ldev.v4l2_name+=dir_entry->d_name;//v4l name for map property
		itermapvl4capdevice it=m_devicesmap.find((char*)devname.data());
		if(it!=m_devicesmap.end())
			devname+="_1";
		printf("Found video input:\"%s\" in %s, driver:%s\n",devname.data(),v4ldev.v4l2_name.data(),v4ldev.driver_version.data());
		m_devicesmap.insert(pairvl4capdevice(devname.data(),v4ldev));
	}
	m_devicesmap_mutex.unlock();
	return 0;
}
int v4lcapture::GetInfoDevice(string path,v4lcapdevice *v4ldev )
{
	int fd= v4l2_open(path.data(), O_RDONLY| O_NONBLOCK);
	if (fd < 0)
	{
		string msg="Error getInfodevice vl4l2_open, "+path;
		printerror(msg);
		return -1;
	}
	struct v4l2_capability cap;
	int er=v4l2_ioctl(fd, VIDIOC_QUERYCAP, &cap);
	if(er<0)
	{
		string msg="Error getInfodevice VIDIOC_QUERYCAP, "+path;
		printerror(msg);
		return -1;
	}
	v4ldev->v4l2_name=(char*)cap.card;
	v4ldev->direction=(char*)cap.bus_info;
	v4ldev->driver_version=(char*)cap.driver;
	return 0;
}
int v4lcapture::GetAvailableResolutions()
{
	if(m_fid==-1)
		return -1;
	struct v4l2_fmtdesc fmt;
	struct v4l2_frmsizeenum frmsize;
	fmt.index = 0;
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	m_resolutions.clear();
	while (v4l2_ioctl(m_fid, VIDIOC_ENUM_FMT, &fmt) >= 0)
	{
			frmsize.pixel_format = fmt.pixelformat;
			string format;//=(char*)fmt.description;
			char * fortxt=(char*)&fmt.pixelformat;
			for(int i=0;i<4;i++)
			format+=fortxt[i];
			frmsize.index = 0;

			vector <resolutionstr> resvector;
			while (v4l2_ioctl(m_fid, VIDIOC_ENUM_FRAMESIZES, &frmsize) >= 0)
			{
				resolutionstr res;
				//res.format=fmt.
				res.type=(v4l2_frmsizetypes)frmsize.type;
				res.pixelformat=frmsize.pixel_format;
				res.flags=fmt.flags;//V4L2_FMT_FLAG_EMULATED/V4L2_FMT_FLAG_COMPRESSED
				struct v4l2_frmivalenum fps;
				memset(&fps,0,sizeof(fps));
				if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE)
				{
					fps.height=frmsize.discrete.height;
					fps.width=frmsize.discrete.width;
					res.discreteresolution=frmsize.discrete;
				}
				else// (frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE)
				{
					fps.height=frmsize.stepwise.max_height;
					fps.width=frmsize.stepwise.max_width;
					res.stepwiseresolution=frmsize.stepwise;
				}

				fps.pixel_format=frmsize.pixel_format;

				while(v4l2_ioctl(m_fid,VIDIOC_ENUM_FRAMEINTERVALS , &fps) >=0)
				{
					frameintervalsstr frm;

					frm.frameratetype=(v4l2_frmivaltypes)fps.type;
					if (fps.type == V4L2_FRMIVAL_TYPE_DISCRETE)
						frm.discretefps=fps.discrete;
					else //(fps.type == V4L2_FRMSIZE_TYPE_STEPWISE)
						frm.stepwisefps=fps.stepwise;
					fps.index++;
					res.frameintervals.push_back(frm);
				}
				frmsize.index++;
				resvector.push_back( res);
			}
			m_resolutions.insert(pairresolution(format,resvector));
			fmt.index++;

	}
	return InitCaptureBuffers(4);

}
int v4lcapture::GetCurrentResolution(	string *vidformat,int *width,int *height,int *hz)
{
	if(m_fid==-1)
		return -1;
	struct v4l2_format format;
	memset(&format,0,sizeof(format));
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(xioctl(m_fid, VIDIOC_G_FMT, &format) < 0)
	{
		string msg="Error GetResolution, "+m_currdevicepath;
		printerror(msg);
		return -1;
	}
	char * fortxt=(char*)&format.fmt.pix.pixelformat;
	m_currentpixelformat=format.fmt.pix.pixelformat;
	m_currentwidth=format.fmt.pix.width;
	m_currentheight=format.fmt.pix.height;
	if(vidformat!=NULL)
		for(int i=0;i<4;i++)
			*vidformat+=fortxt[i];
	if(width!=NULL)
		*width=format.fmt.pix.width;
	if(height!=NULL)
		*height=format.fmt.pix.height;

	/*if(xioctl(m_fid, VIDIOC_G_FRAMEINTERVAL, &tofivalformat) < 0)//NOT IMPLEMENTED IN V4L YET
	 {
		string msg="Error GetResolution, "+m_currdevicepath;
		printerror(msg);
		//return -1;
	}*/
	if(hz!=NULL)
		*hz=-1;

	return 0;
}
string v4lcapture::GetResolutionsInformation()
{
	string res;
	res+="Device:"+m_current_device.v4l2_name+"\n";
	res+="\tRESOLUTIONS:\n";
	char aux[500];
	for(itermapresolution iter=m_resolutions.begin();iter != m_resolutions.end();iter++)
	{
		vector <resolutionstr> vt=iter->second;
		res+="\t"+iter->first+"\n";

		for(vector <resolutionstr>::iterator vtiter=vt.begin();vtiter!=vt.end();vtiter++)
		{
			resolutionstr dat=*vtiter;
			char* cuac=(char*)&dat.pixelformat;
			if(dat.type==V4L2_FRMSIZE_TYPE_DISCRETE)
				sprintf(aux,"\t\tDiscrete: %d,%d (%c%c%c%c)\n",dat.discreteresolution.width,dat.discreteresolution.height,
								cuac[0],cuac[1],cuac[2],cuac[3]);
			else
				sprintf(aux,"\t\tStepwise: from %dx%d to %dx%d ,step:%dx%d\n",dat.stepwiseresolution.min_width,dat.stepwiseresolution.min_height,
								dat.stepwiseresolution.max_width,dat.stepwiseresolution.max_height,
								dat.stepwiseresolution.step_width,dat.stepwiseresolution.step_height);
			res+=aux;
			for(vector <frameintervalsstr>::iterator vfiter=dat.frameintervals.begin();vfiter!=dat.frameintervals.end();vfiter++)
			{
				 frameintervalsstr fat=*vfiter;
				if(fat.frameratetype==V4L2_FRMIVAL_TYPE_DISCRETE)
					sprintf(aux,"\t\t\tFPS Discrete: %d/%d second\n",fat.discretefps.numerator,fat.discretefps.denominator);
				else
					sprintf(aux,"\t\t\tFPS Stepwise: from %d/%d to %d/%d ,step:%d/%d\n",fat.stepwisefps.min.numerator,fat.stepwisefps.min.denominator,
									fat.stepwisefps.max.numerator,fat.stepwisefps.max.denominator,
									fat.stepwisefps.step.numerator,fat.stepwisefps.step.denominator);
				res+=aux;
			}
		}

	}
return res;

}
int v4lcapture::SetResolution(/*resolutionstr videoformat*/string standard, int width, int height)
{
	if(m_fid==-1)
		return -1;
//	if(m_status== V4LCAP_RUN || m_status!=  V4LCAP_PAUSED)//only change resolutions if not V4LCAP_RUN or V4LCAP_PAUSED
//		return -1;
	struct v4l2_format format;
	memset(&format,0,sizeof(format));
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(xioctl(m_fid, VIDIOC_G_FMT, &format) < 0){
		string msg="Error GetResolution, "+m_currdevicepath;
		printerror(msg);
		//return -1;
	}
	char *c=(char*)&format.fmt.pix.pixelformat;
	printf("Current video format=%c%c%c%c (%dx%d)\n",c[0],c[1],c[2],c[3],format.fmt.pix.width,format.fmt.pix.height);
	if(standard.size()<4)
		return -1;
	__u32 pxfr;
	char* pxs=(char*)&pxfr;
	for(int r=0;r<4;r++)
		pxs[r]=standard.at(r);
	memset(&format,0,sizeof(format));
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	format.fmt.pix.pixelformat = pxfr;
	format.fmt.pix.width = width;
	format.fmt.pix.height = height;
	format.fmt.pix.field = V4L2_FIELD_INTERLACED;
	ReleaseCaptureBuffers();
	if(xioctl(m_fid, VIDIOC_S_FMT, &format) < 0)
	{
		string msg="Error SetResolution, VIDIOC_S_FMT "+m_currdevicepath;
		printerror(msg);
		return -1;
	}
	if(xioctl(m_fid, VIDIOC_G_FMT, &format) < 0)
	{
		string msg="Error GetResolution, VIDIOC_G_FMT "+m_currdevicepath;
		printerror(msg);
		return -1;
	}
	c=(char*)&format.fmt.pix.pixelformat;
	printf("New video format=%c%c%c%c (%dx%d)\n",c[0],c[1],c[2],c[3],format.fmt.pix.width,format.fmt.pix.height);
	m_currentpixelformat=format.fmt.pix.pixelformat;
	m_currentwidth=format.fmt.pix.width;
	m_currentheight=format.fmt.pix.height;
	return 0;
}
int v4lcapture::GetCurrentControls()
{
	if(m_fid==-1)
		return -1;
	m_controls.clear();
	struct v4l2_queryctrl queryctrl;
	memset (&queryctrl, 0, sizeof (queryctrl));
	for (queryctrl.id = V4L2_CID_BASE; queryctrl.id < V4L2_CID_LASTP1; queryctrl.id++)
	{
		string ctrname;
		controlstr strc;

		if (0 == xioctl (m_fid, VIDIOC_QUERYCTRL, &queryctrl))
		{
			if(GetControlsInfo(&ctrname,&strc,queryctrl)==0)
				m_controls.insert(paircontrolstr(ctrname,strc));
		}
		else
		{
			if(errno==EINVAL)
				continue;
			string msg="Error getCurrControls VIDIOC_QUERYCTRL, "+m_currdevicepath;
			printerror(msg);
		}
	}
	for (queryctrl.id = V4L2_CID_PRIVATE_BASE;; queryctrl.id++)
	{
		string ctrname;
		controlstr strc;
		if (0 == xioctl (m_fid, VIDIOC_QUERYCTRL, &queryctrl))
		{
			if(GetControlsInfo(&ctrname,&strc,queryctrl)==0)
				m_controls.insert(paircontrolstr(ctrname,strc));
		}
		else
			if (errno == EINVAL)
				break;
	}
	queryctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
	while (0 == xioctl (m_fid, VIDIOC_QUERYCTRL, &queryctrl))
	{
		string ctrname;
		controlstr strc;
		if(GetControlsInfo(&ctrname,&strc,queryctrl)==0)
			m_controls.insert(paircontrolstr(ctrname,strc));
		queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
	}

	return 0;
	//V4L2_CTRL_TYPE_INTEGER
}
int v4lcapture::UpdateControls()
{
	if(m_fid==-1)
		return -1;
	for(itermapcontrolstr iter=m_controls.begin();iter!=m_controls.end();iter++)
	{
		controlstr *cur=&iter->second;
		UpdateControlInfo(cur);
	}
	return 0;
}
int v4lcapture::UpdateControlInfo(controlstr *strc)
{
	struct v4l2_queryctrl queryctrl;
	memset (&queryctrl, 0, sizeof (queryctrl));
	queryctrl.id=strc->id;
	string ctrname;
	if (0 == xioctl (m_fid, VIDIOC_QUERYCTRL, &queryctrl))
	{
		if(GetControlsInfo(&ctrname,strc,queryctrl)==0)
		return 0;
	}
	string msg="Error getCurrControls VIDIOC_QUERYCTRL, "+m_currdevicepath;
	printerror(msg);
	return -1;
}
int v4lcapture::IsControlEnabled(string name)
{
	struct v4l2_queryctrl queryctrl;
	memset (&queryctrl, 0, sizeof (queryctrl));
	controlstr *cur=&(m_controls.find(name))->second;
	queryctrl.id=cur->id;
	string ctrname;
	if (0 == xioctl (m_fid, VIDIOC_QUERYCTRL, &queryctrl))
		return !IS_CONTROL_DISABLED(queryctrl.flags);
	string msg="Error getCurrControls VIDIOC_QUERYCTRL, "+m_currdevicepath;
	printerror(msg);
	return -1;
}
int v4lcapture::GetControlsInfo(string *ctrname, controlstr *strc, const v4l2_queryctrl queryctrl)
{
	if(m_fid==-1)
		return -1;
	//printf("control :%s\n",(char*)queryctrl.name);
	*ctrname=(char*)queryctrl.name;
	strc->type=(v4l2_ctrl_type)queryctrl.type;
	strc->id=queryctrl.id;
	if (queryctrl.type == V4L2_CTRL_TYPE_MENU || V4L2_CTRL_TYPE_INTEGER_MENU==queryctrl.type)//menu->stract options
	{
		struct v4l2_querymenu querymenu;
		memset(&querymenu,0,sizeof(querymenu));
		querymenu.id = queryctrl.id;
		for (querymenu.index =(__u32) queryctrl.minimum; querymenu.index <= (__u32)queryctrl.maximum; querymenu.index++)
		{
			if (0 == xioctl (m_fid, VIDIOC_QUERYMENU, &querymenu))
			{
				if(queryctrl.type == V4L2_CTRL_TYPE_MENU)
					strc->data.push_back((string)(char*)querymenu.name);
				if(queryctrl.type == V4L2_CTRL_TYPE_INTEGER_MENU)
				{
					char ms[250];
					snprintf(ms,250,"%lli",querymenu.value);
					strc->data.push_back((string)ms);
				}
				strc->menuindex.push_back(querymenu.index);
			}
			else
			{
				string msg="Error GetControlInfo VIDIOC_QUERYMENU, "+m_currdevicepath;
				printerror(msg);
				continue;
			}
		}
		strc->flags=queryctrl.flags;
		strc->defvalue=queryctrl.default_value;
		int cont=0;
		for(std::vector<int>::iterator iter=strc->menuindex.begin();iter!=strc->menuindex.end();iter++,cont++)
		if(*iter==queryctrl.default_value)
			break;
		strc->defvalue=cont;
	}

	if(queryctrl.type == V4L2_CTRL_TYPE_INTEGER || queryctrl.type == V4L2_CTRL_TYPE_INTEGER64
		 /*||queryctrl.type ==V4L2_CTRL_TYPE_U8
		 ||queryctrl.type == V4L2_CTRL_TYPE_U16*/)
	{
		strc->max=queryctrl.maximum;
		strc->min=queryctrl.minimum;
		strc->defvalue=queryctrl.default_value;
		strc->flags=queryctrl.flags;
		strc->step=queryctrl.step;
	}
	if(queryctrl.type ==V4L2_CTRL_TYPE_BOOLEAN)
	{
		strc->max=1;
		strc->min=0;
		strc->defvalue=queryctrl.default_value;
		strc->flags=queryctrl.flags;
	}
	return 0;
}
string v4lcapture::GetControlsInformation()
{
	string res;
	res+="Device:"+m_current_device.v4l2_name+"\n";
	res+="\tCONTROLS:\n";
	char aux[500];
	for(itermapcontrolstr iter=m_controls.begin();iter!=m_controls.end();iter++)
	{
		controlstr cur=iter->second;
		res+=iter->first;
		if(cur.type== V4L2_CTRL_TYPE_INTEGER || cur.type == V4L2_CTRL_TYPE_INTEGER64
			 /*||cur.type ==V4L2_CTRL_TYPE_U8
			 ||cur.type == V4L2_CTRL_TYPE_U16*/)
		{
			res+="\t(int) ";
			sprintf(aux,"from %d to %d step:%d default=%ld",cur.min,cur.max,
							cur.step,cur.defvalue)	;
			res+=aux;
		}
		if (cur.type == V4L2_CTRL_TYPE_MENU || V4L2_CTRL_TYPE_INTEGER_MENU==cur.type)
		{
			res+="\t(menu) options ";
			int n=0;
			if(cur.data.size()!=cur.menuindex.size())
				continue;//string options and index have to have same size
			for(vector <string>::iterator itr=cur.data.begin();itr!=cur.data.end();itr++)
			{
				sprintf(aux,"%d(%s), ",cur.menuindex.at(n),(*itr).data());
				res+=aux;
				n++;
			}
			sprintf(aux," default= %ld",cur.defvalue);
			res+=aux;
		}
		if (cur.type == V4L2_CTRL_TYPE_BOOLEAN)
		{
			sprintf(aux,"\t(bool) default= %ld",cur.defvalue);
			res+=aux;
		}
		if(cur.flags &V4L2_CTRL_FLAG_INACTIVE )
			res+=" INACTIVE!!\n";
		else
			res+="\n";

	}
	return res;
}
int v4lcapture::GetControlValue(string ctrlname,int *curval)
{
	if(m_fid==-1||curval==NULL)
		return -1;
	controlstr *cur=&(m_controls.find(ctrlname))->second;
	struct v4l2_control control;
	memset (&control, 0, sizeof (control));
	control.id = cur->id;

	if (-1 == xioctl (m_fid, VIDIOC_G_CTRL, &control))
	{
		string msg="Error GetControlValue , " + ctrlname + " "+ m_currdevicepath;
		printerror(msg);
		return -1;
	}
	if(cur->type==V4L2_CTRL_TYPE_MENU || cur->type==V4L2_CTRL_TYPE_INTEGER_MENU)
	{
		*curval=0;
		int cont;
		for(std::vector<int>::iterator iter=cur->menuindex.begin();iter!=cur->menuindex.end();iter++,cont++)
		if(*iter==control.value)
			break;
		*curval =cont;
		return 0;
	}
	*curval=control.value  ;
	return 0;
}
int v4lcapture::SetControlValue(string ctrlname, int newval)
{
	if(m_fid==-1)
		return -1;
	controlstr *cur=&m_controls.find(ctrlname)->second;
	if(cur==NULL)
		return -1;
	struct v4l2_control control;
	memset (&control, 0, sizeof (control));
	control.id = cur->id;
	if(cur->type==V4L2_CTRL_TYPE_MENU || cur->type==V4L2_CTRL_TYPE_INTEGER_MENU)
	{
		if(newval<(int)cur->menuindex.size())
			control.value = cur->menuindex.at(newval);//set value of vector at index ()
		else
			control.value=newval;
	}
	else
		control.value=newval;
	if (-1 == xioctl (m_fid, VIDIOC_S_CTRL, &control))
	{
		string msg="Error SetControlValue , " + ctrlname + m_currdevicepath;
		printerror(msg);
		return -1;
	}
	return 0;
}
/*bool v4lcapture::GetMaxResolutions(int fd,v4lcapdevice *v4ldev)
{
	if(fd<0)
		return -1;
	struct video_capability vcap;
	int er=v4l2_ioctl(fd,VIDIOC_GCAP , &vcap);
	if(er<0)
	{
		printerror((char*)path.data());
		close(fd);
		return -1;
	}
	v4ldev->min_resolution[0]=vcap.minwidth;
	v4ldev->min_resolution[1]=vcap.maxheight;
	v4ldev->max_resolution[0]=vcap.maxwidth;
	v4ldev->max_resolution[1]=vcap.height;
}*/
int v4lcapture::InitCaptureBuffers(int nbuffers)
{
	if(m_fid==-1)
		return -1;
	/*struct v4l2_requestbuffers      req;
		struct v4l2_buffer              buf;
	memset(&req, 0, sizeof(req));
		req.count = 2;

	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;
	xioctl(m_fid, VIDIOC_REQBUFS, &req);
	for (int n_buffers = 0; n_buffers < req.count; ++n_buffers) {
				memset(&buf, 0, sizeof(buf));

					buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
					buf.memory      = V4L2_MEMORY_MMAP;
					buf.index       = n_buffers;

					xioctl(m_fid, VIDIOC_QUERYBUF, &buf);

					int length = buf.length;
					char* start =(char*) v4l2_mmap(NULL, buf.length,
												PROT_READ | PROT_WRITE, MAP_SHARED,
												m_fid, buf.m.offset);

					if (MAP_FAILED == start) {
									perror("mmap");

					}
	}

*/
	struct v4l2_requestbuffers reqbuf;
	m_v4l2buffers.clear();
	unsigned int i;
	struct v4l2_format format;
	memset(&format,0,sizeof(format));
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(xioctl(m_fid, VIDIOC_G_FMT, &format) < 0)//getting capture information
	{
		string msg="Error Initbuffers g_fmt, "+m_currdevicepath;
		printerror(msg);
		return -1;
	}
	memset(&reqbuf, 0, sizeof(reqbuf));
	reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	reqbuf.memory = V4L2_MEMORY_MMAP;
	reqbuf.count =nbuffers;
	if (-1 == xioctl (m_fid, VIDIOC_REQBUFS, &reqbuf))
	{
		if (errno == EINVAL)
		{
			printf("Video capturing or mmap-streaming is not supported\n");
			string msg="Error  InitCaptureBuffers, Video capturing or mmap-streaming is not supported" + m_currdevicepath;
			printerror(msg);
		}
		else
		{
			string msg="Error  InitCaptureBuffers, VIDIOC_REQBUFs" + m_currdevicepath;
			printerror(msg);
			return -1;
		}
	}
	if (reqbuf.count ==0)
	{
		string msg="Not enough buffer memory, VIDIOC_REQBUFs" + m_currdevicepath;
		printerror(msg);
		return -1;
	}
	for (i = 0; i < reqbuf.count; i++)
	{
		struct v4l2_buffer buffer;
		memset(&buffer, 0, sizeof(buffer));
		buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buffer.memory = V4L2_MEMORY_MMAP;
		buffer.index = i;
		if (-1 == xioctl (m_fid, VIDIOC_QUERYBUF, &buffer))
		{
			string msg="Error  InitCaptureBuffers, VIDIOC_QUERYBUF" + m_currdevicepath;
			printerror(msg);
		}
		strv4l2buffer buf;
		buf.pointer = (char*) v4l2_mmap(NULL, buffer.length,
												PROT_READ | PROT_WRITE, MAP_SHARED,
												m_fid, buffer.m.offset);
		buf.height=format.fmt.pix.height;
		buf.width=format.fmt.pix.width;
		buf.bytesline=format.fmt.pix.bytesperline;
		buf.bytespixel=buf.bytesline/buf.width;
		buf.length = buffer.length;
		if (MAP_FAILED == buf.pointer)
		{
			string msg="Error  mmap" + m_currdevicepath;
			printerror(msg);
			continue;
		}
		else
			m_v4l2buffers.push_back(buf);
	}
	this->m_nbuffers=i;
	if(i>0)
		m_status=V4LCAP_PAUSED;
	return 0;
}
int v4lcapture::ReleaseCaptureBuffers()
{
	if(m_fid==-1||m_status==V4LCAP_CLOSED)
		return -1;
	for (vector <strv4l2buffer>::iterator i = m_v4l2buffers.begin(); i != m_v4l2buffers.end(); i++)
	{
		if (-1 ==v4l2_munmap((*i).pointer, (*i).length))
		{
			string msg="Error  Unmapping memory " + m_currdevicepath;
			printerror(msg);

		}
	}
	m_v4l2buffers.clear();
	fsync(m_fid);
	struct v4l2_requestbuffers reqbuf;
	memset(&reqbuf, 0, sizeof(reqbuf));
	reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	reqbuf.memory = V4L2_MEMORY_MMAP;
	reqbuf.count =0;
	if (-1 == xioctl (m_fid, VIDIOC_REQBUFS, &reqbuf))//neccesary
	{
		if (errno == EINVAL)
		{
			printf("Video capturing or mmap-streaming is not supported\n");
			string msg="Error  InitCaptureBuffers, Video capturing or mmap-streaming is not supported" + m_currdevicepath;
			printerror(msg);
		}
		else
		{
			string msg="Error  ReleaseCaptureBuffers, VIDIOC_REQ 0 BUF " + m_currdevicepath;
			printerror(msg);
		}
	}
	m_status=V4LCAP_UNCONFIGURED;
	return 0;
}
int v4lcapture::StartAdquisition()
{
	m_fpsclock.tv_sec=m_fpsclock.tv_usec=0;
	m_nframesfps=0;
	m_realfps=0;
	if(m_fid==-1|| m_status==V4LCAP_UNCONFIGURED||m_status==V4LCAP_CLOSED)
		return -1;
	for(int i = 0; i < m_nbuffers; ++i)
	{
		struct v4l2_buffer bufferinfo;
		memset(&bufferinfo, 0, sizeof(bufferinfo));
		bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		bufferinfo.memory = V4L2_MEMORY_MMAP;
		bufferinfo.index = i;
		if(0!=xioctl(m_fid, VIDIOC_QBUF, &bufferinfo))
		{
				string msg="Error  VIDIOC_QBUF " + m_currdevicepath;
				printerror(msg);
		}
	}
	int type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(xioctl(m_fid, VIDIOC_STREAMON, &type) < 0)
	{
		string msg="Error starting capture, VIDIOC_STREAMON " + m_currdevicepath;
		printerror(msg);
		return -1;
	}
	m_status=V4LCAP_RUN;
	return 0;
}
int v4lcapture::StopAdquisition()
{
	if(m_fid==-1|| m_status!=V4LCAP_RUN)
		return -1;
	if(!m_capturesemaphore.tryAcquire(1,2500))
	{
		string msg="Error Capturing semaphore to stop";
		printerror(msg);
	}
	if(m_adquiring==1)
		m_adquiring=0;
	int type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(0!= xioctl(m_fid, VIDIOC_STREAMOFF, &type))
	{
		string msg="Error stopping capture, VIDIOC_STREAMON" + m_currdevicepath;
		printerror(msg);
		return -1;
	}
/*	for(int i = 0; i < m_nbuffers; ++i)//dequeue buffers
	{
		struct v4l2_buffer bufferinfo;
		memset(&bufferinfo, 0, sizeof(bufferinfo));
		bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		bufferinfo.memory = V4L2_MEMORY_MMAP;
		bufferinfo.index = i;
		if(0!=xioctl(m_fid, VIDIOC_DQBUF, &bufferinfo))
		{
				string msg="Error  VIDIOC_DQBUF " + m_currdevicepath;
				printerror(msg);
		}
	}*/

	m_status=	V4LCAP_PAUSED;
	m_capturesemaphore.release();
	return 0;
}
int v4lcapture::WaitNextFrame(v4l2image **img,int mstimeout)
{
	if(m_fid==-1|| m_status!=V4LCAP_RUN)
		return -1;
	if(m_capturesemaphore.tryAcquire(1,100)==0 )
	 return -1;
	m_adquiring=1;
	fd_set    fds ;
	struct timeval   tv;
	FD_ZERO(&fds);
	FD_SET(m_fid,  &fds);
	tv.tv_sec= 0;       /* Timeout. */
	tv.tv_usec= mstimeout*1000;
	int iret= select(m_fid+ 1, &fds, NULL, NULL, &tv);
	if(iret<=0)
	{
		*img=NULL;
		m_capturesemaphore.release();
		return -1;
	}
	struct v4l2_buffer tV4L2buf;
	memset(&tV4L2buf, 0, sizeof(struct v4l2_buffer));
	tV4L2buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	tV4L2buf.memory = V4L2_MEMORY_MMAP;
	if(0!=xioctl(m_fid, VIDIOC_DQBUF, &tV4L2buf))
	{
		string msg="Error dequeue buffer during  capture,  VIDIOC_DQBUF" + m_currdevicepath;
		printerror(msg);
		*img=NULL;
		m_capturesemaphore.release();
		m_adquiring=0;
		return -1;
	}
	*img=new v4l2image(tV4L2buf.bytesused);
	gettimeofday(&((*img)->acqtime), NULL);
	CalculateFPS();
	//v4l2_field field=(v4l2_field)tV4L2buf.field;


	/*struct v4l2_format format;
	memset(&format,0,sizeof(format));
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(xioctl(m_fid, VIDIOC_G_FMT, &format) < 0){
		string msg="Error GetResolution, "+m_currdevicepath;
		printerror(msg);
		//return -1;
	}*/
//	char *c=(char*)&format.fmt.pix.pixelformat;
	//printf("New video format=%c%c%c%c (%dx%d)",c[0],c[1],c[2],c[3],format.fmt.pix.width,format.fmt.pix.height);
	memcpy((*img)->pointer,m_v4l2buffers[tV4L2buf.index].pointer,tV4L2buf.bytesused);
	(*img)->length=m_v4l2buffers[tV4L2buf.index].length;
	(*img)->height=m_v4l2buffers[tV4L2buf.index].height;
	(*img)->width=m_v4l2buffers[tV4L2buf.index].width;
	(*img)->bytesline=m_v4l2buffers[tV4L2buf.index].bytesline;
	(*img)->bytespixel=m_v4l2buffers[tV4L2buf.index].bytespixel;
	char* pxf=(char*)&m_currentpixelformat;
	for(int r=0;r<4;r++)
	(*img)->pixformat[r]=pxf[r];
/*
	int jpgfile;
	if((jpgfile = open("/home/rocabo/myimage.jpeg", O_WRONLY | O_CREAT, 0660)) < 0){
			perror("open");
	}	write(jpgfile,(*img)->pointer, tV4L2buf.length);
	close(jpgfile);*/
/*
	FILE* fout = fopen("/home/rocabo/myimage.ppm", "w");
	if (fout) {
		fprintf(fout, "P6\n%d %d 255\n",
							m_v4l2buffers[tV4L2buf.index].width, m_v4l2buffers[tV4L2buf.index].height);
			fwrite(m_v4l2buffers[tV4L2buf.index].pointer,(*img)->length , 1, fout);
			fclose(fout);
	}
*/
	if(ioctl(m_fid, VIDIOC_QBUF, &tV4L2buf) < 0)
	{
		string msg="Error queue buffer during  capture,  VIDIOC_QBUF" + m_currdevicepath;
		printerror(msg);
		*img=NULL;
		m_capturesemaphore.release();
		m_adquiring=0;
		return -2;
	}
	m_capturesemaphore.release();
	m_adquiring=0;
	return 0;
}
void v4lcapture::CalculateFPS()
{
	if(m_nframesfps==NFRAMES_FPS_INTERVAL-1)
	{
		struct timeval clkaux ;
		gettimeofday(&clkaux, NULL);
		double enlapsedsecs =	(double)(clkaux.tv_sec*1000+clkaux.tv_usec/1000)-(m_fpsclock.tv_sec*1000+
				m_fpsclock.tv_usec/1000);

		if(enlapsedsecs>0)
			m_realfps=1.0/(enlapsedsecs/1000/NFRAMES_FPS_INTERVAL);
		m_fpsclock=clkaux;
		m_nframesfps=0;
	}
	else
		m_nframesfps++;
}
void v4lcapture::printerror(string device)
{
		m_lasterror=device+ ": failed: "+ strerror(errno);
		printf("%s\n", m_lasterror.data());

}
