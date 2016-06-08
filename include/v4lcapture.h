/*
 * v4lcapture.h: Linux v4l camera adquisition library/ program viewer.
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
	\brief class to interface with v4l camera adquisition library store information into structures.
	\date 05-2015
	\author mperez
*/
#ifndef V4LCAPTURE_H
#define V4LCAPTURE_H

#include <map>
#include <string>
#include <stdio.h>
#include "stdio.h"
#include <vector>
#include <inttypes.h>
#include <sys/types.h>
#include "globmutex.h"
#include "globalsemaphore.h"
#include <linux/videodev2.h>
#include <time.h>

#define IS_CONTROL_DISABLED(X) (X==V4L2_CTRL_FLAG_DISABLED || X==V4L2_CTRL_FLAG_INACTIVE||X==V4L2_CTRL_FLAG_READ_ONLY)
#define NFRAMES_FPS_INTERVAL 5		//!< NUMBER OF FRAMES TO CALCULATE REAL FPS

using namespace std;
///GENERAL INFORMATION
enum devstatus{V4LCAP_CLOSED=0,V4LCAP_UNCONFIGURED=1,V4LCAP_CONFIGURED,V4LCAP_RUN,V4LCAP_PAUSED };//!< CAMERA STATUS
/**
 * @brief The v4lcapdevice class Capture device class only for info.
 */
class v4lcapdevice  // VIDIOC_QUERYCAP
{
public:
	/// Device information
	string	v4l2_name;					//!< Device name
	//string	device_name;			//!< V4L device name  map identifier
	string	driver_version;			//!< Version of the driver behind the device
	string	direction;					//!< bus direction
	/*int max_resolution[2];		//!< Max device resolution
	int min_resolution[2];			//!< Min device resolution*/
};
/**
 * @brief The v4l2image class Image class, for image cross between classes.
 */
class v4l2image{
public:
	v4l2image(int length){pointer =new char[length];};
	~v4l2image(){if(pointer!=NULL)delete []pointer;};
	char *pointer;				//!< Data pointer.
	int width;						//!< Image width in pixels.
	int height;						//!< Image heigth in pixels.
	int bytesline;				//!< Bytes per line.
	size_t length;				//!< Buffer total lenght.
	int bytespixel;				//!< Bits per pixel
	char pixformat[4];		//!< Pixel format
	timeval acqtime;			//!< Adquisition time
};

typedef struct v4l2buffer{
	char* pointer;			//!< Data pointer.
	size_t length;			//!< Image width in pixels.
	int width;					//!< Image width in pixels.
	int height;					//!< Image heigth in pixels.
	int bytespixel;			//!< Bits per pixel
	int bytesline;			//!< Buffer total lenght.
}strv4l2buffer;	//!< Struct for v4l2 memory buffers.

/*typedef struct vl4capdevice
{
	v4lcapdevice device;
}vl4capdevicestr;	//!<*/

typedef map		<string,v4lcapdevice> mapvl4capdevice;			//!< Map for devices information.
typedef pair	<string,v4lcapdevice> pairvl4capdevice;
typedef map		<string,v4lcapdevice>::iterator itermapvl4capdevice;

///RESOLUTIONS INFORMATION
typedef struct strframeintervals{
	v4l2_frmivaltypes frameratetype;// 0 discrete 1 stepwise
	union {					/* Frame interval */  // TODO SHOULD BE A VECTOR...
		struct v4l2_fract		discretefps;						//!< Like v4l constant values
		struct v4l2_frmival_stepwise	stepwisefps;	//!< Like v4l multiple selection
	};
}frameintervalsstr;	//!< Struct for fps information.

typedef struct resolutions
{
	v4l2_frmsizetypes type;	//!<  0 discrete 1 stepwise
	__u32 pixelformat;			//!<   Like v4l char[4] pixel information
	__u32 flags;						//!<  Like v4l V4L2_FMT_FLAG_COMPRESSED(device native) or V4L2_FMT_FLAG_EMULATED
	union {					/* Frame size */
		struct v4l2_frmsize_discrete	discreteresolution;	//!< Like v4l constant values
		struct v4l2_frmsize_stepwise	stepwiseresolution;	//!< Like v4l multiple selection
	};
	vector <frameintervalsstr> frameintervals;
}resolutionstr;	//!< Struct with information about format resolution and fps (RGB32,640x480 50hz...)

typedef map		<string,vector <resolutionstr> > mapresolution;//!< Map for resolutions string = format(MPEG,YUV) with a vector of resolutions (dimensions and fps)
typedef pair	<string,vector <resolutionstr> > pairresolution;
typedef map		<string,vector <resolutionstr> >::iterator itermapresolution;

///CONTROLS INFORMATION
///
typedef struct control
{
	v4l2_ctrl_type type;		//!< V4l internals
	int id;									//!< V4l internals
	int min;								//!< Min value.
	int max;								//!< Max value
	int step;								//!< Stept.
	long value;							//!< Actual value.
	long defvalue;					//!< Default value.
	int flags;							//!< V4l flags (inactive,DISABLED, READONLY..)
	vector <int> menuindex;	//!< if menu type menu index.
	vector <string> data;		//!< if menu type menus, if other(int/int..) name.
}controlstr;//!< Struct for camera controls
typedef map		<string,controlstr > mapcontrols;//!< Map for controls name and structure information
typedef pair	<string,controlstr  >paircontrolstr;
typedef map		<string,controlstr >::iterator itermapcontrolstr;
/**
 * @brief The v4lcapture class. Class for interfacing to V4l. Store information in maps.
 */
class v4lcapture
{
///****************************************** FUNCIONES **************************************
///*******************************************************************************************
public:
	v4lcapture();
	~v4lcapture();
	/**
	 * @brief OpenDevice
	 * @param path Complete path.
	 * @return
	 */
	int OpenDevice(const char *path);
	/**
	 * @brief Open Device by device name.
	 * @param name Device name (from devices map).
	 * @return
	 */
	int OpenDeviceName(char *name);
	/**
	 * @brief CloseDevice
	 * @return
	 */
	int CloseDevice();
	/** @brief UpdateDevicesMap updates common devices list.
	 *	@return
	 */
	/**
	 * @brief Update map of all capture devices information.
	 * @return
	 */
	int UpdateDevicesMap();
	/**
	 * @brief Print data from errno value.(internal use).
	 * @param device Path to device.
	 */
	void printerror(string device);
	string GetLastErrorstr(){return m_lasterror;};
	/**
	 * @brief Get information about specific device
	 * @param path Total path.
	 * @param v4ldev Struct to store information.
	 * @return
	 */
	int GetInfoDevice(string path, v4lcapdevice *v4ldev);
	//static int GetMaxResolutions(int fd, v4lcapdevice *v4ldev);
	/**
	 * @brief Get available resolutions from actual oppened device.
	 * @return
	 */
	int GetAvailableResolutions();
	/**
	 * @brief Get available controls from actual oppened device.
	 * @return
	 */
	int GetCurrentControls();
	/**
	 * @brief Update controls map with current information.
	 * @return 0
	 */
	int UpdateControls();
	/**
	 * @brief Updata information about a control.
	 * @param strc Control existing structure .
	 * @return 0 noerror -1 for error.
	 */
	int UpdateControlInfo(controlstr *strc);
	/**
	 * @brief Returns if a specific control is enabled.
	 * @param name control name.
	 * @return 1 enabled 0 disabled -1 error.
	 */
	int IsControlEnabled(string name);
	/**
	 * @brief Stract information about specific control from v4l2 struct.
	 * @param ctrname Control name.
	 * @param strc	Control structure to fill.
	 * @param queryctrl Control data.
	 * @return
	 */
	int GetControlsInfo(string *ctrname, controlstr *strc, const v4l2_queryctrl queryctrl);
	/**
	 * @brief Get information about current configuration.
	 * @param type chars type 4char format
	 * @return 0 ok -1 error.
	 */
	int GetCurrentResolution(string *vidformat, int *width, int *height, int *hz);
	/**
	 * @brief Prints resolutions info into a string .
	 * @return
	 */
	string GetResolutionsInformation();
	/**
	 * @brief Prints controls info into a string .
	 * @return
	 */
	string GetControlsInformation();
	/**
	 * @brief Get actual control value.
	 * @param ctrlname Control name.
	 * @param curval Pointer to fill.
	 * @return
	 */
	int GetControlValue(string ctrlname, int *curval);
	/**
	 * @brief Set actual control value.
	 * @param ctrlname Control name.
	 * @param newval New value.integer or menu index(for menus)
	 * @return
	 */
	int SetControlValue(string ctrlname, int newval);
	/**
	 * @brief Init capture buffers.
	 * @param nbuffers Number of buffers.
	 * @return
	 */
	int InitCaptureBuffers(int nbuffers);
	/**
	 * @brief Release Capture Buffers
	 * @return
	 */
	int ReleaseCaptureBuffers();
	/**
	 * @brief Starts Adquisition
	 * @return
	 */
	int StartAdquisition();
	/**
	 * @brief Stops Adquisition
	 * @return
	 */
	int StopAdquisition();
	/**
	 * @brief Wait until next frame
	 * @param img Image to fill.
	 * @param mstimeout Timeout
	 * @return
	 */
	int WaitNextFrame(v4l2image **img,int mstimeout);
	/**
	 * @brief Change resolution.
	 * @param val
	 * @param width
	 * @param height
	 * @return
	 */
	int SetResolution(string standard, int width, int height);
	/**
	 * @brief V4l internal ioctl.
	 * @param fh
	 * @param request
	 * @param arg
	 * @return
	 */
	static	int xioctl(int fh, int request, void *arg);
private:
	/**
	 * @brief Calculate real FPS every NFRAMES_FPS_INTERVAL frames.
	 */
	void CalculateFPS();
///****************************************** VARIABLES **************************************
///*******************************************************************************************
private:
	int m_fid;															//!< Current device fid.
public:
	static mapvl4capdevice  m_devicesmap;		//!< Map containing all devices.
	static globmutex m_devicesmap_mutex;		//!< Mutex for accesing devices map(from all class subinstances).
	globalsemaphore m_capturesemaphore;			//!< Semaphore for capturing.
	string m_currdevicepath;								//!< Current device path.
	string m_lasterror;											//!< Last error.
	mapresolution m_resolutions;						//!< Current device resolutons.
	mapcontrols m_controls;									//!< Current controls.
	v4lcapdevice m_current_device;					//!< Current device information.
	devstatus m_status;											//!< Current status.
	int m_nbuffers;													//!< Number of adquisition buffers.
	vector <strv4l2buffer> m_v4l2buffers;		//!< Adquisition buffers information
	__u32 m_currentpixelformat;							//!< Curent pixel format
	int m_currentwidth;											//!< Current widht
	int m_currentheight;										//!< Current height
	struct timeval  m_fpsclock;							//!< Clock for calculating real fps
	int m_nframesfps;
	double m_realfps;
	bool m_adquiring;
};

#endif // V4LCAPTURE_H
