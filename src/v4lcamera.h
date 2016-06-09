/*
 * v4lcamera.cpp: Linux v4l camera adquisition library/ program viewer.
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
	Sample code:
 \code
	v4lcamera vl(NULL,0);//create class working as program >> create a control dialog
	v4l2image *img;//image instance
	vl.WaitNextFrame(&img,290);//get the image
	\endcode
*/
#ifndef V4LCAMERA_H
#define V4LCAMERA_H

#include <QMainWindow>
#include "v4lcapture.h"
#include <qlabel.h>
#include <qpushbutton.h>
#include <qcombobox.h>
#include <QDoubleSpinBox>
#include <qcheckbox.h>
#include <qsignalmapper.h>
#include <qgridlayout.h>
#include <QTimer>
#define MAX_CAMS 15
namespace Ui {
class v4lcamera;
}

/** \class v4lcamera
		\brief Adquisition / main aplication to adquire images from cameras using v4l library.*/
class v4lcamera : public QMainWindow, public  v4lcapture
{
	Q_OBJECT

public:
	explicit v4lcamera(QWidget *parent = 0,bool workaslibrary=1);
	~v4lcamera();
	/**
	 *  Updates Combobox with available devices.
	 */
	void UpdateComboDevices();
	/**
	 *  Updates combobox with available standards.
	 */
	void UpdateComboStandards();
	/**
	 *  Updates combobox with available resolutions.
	 */
	void UpdateComboResolutions();
	/**
	 * Creates camera available controls.
	 */
	void CreateControls();
	/**
	 * @brief Resend new image from adquisition thread when working as capture program.
	 * @param img new image.
	 */
	void EmitNewImage(v4l2image *img){emit SGNewImage(img);};
	/**
	 * @brief Update the control values and visibility into the dialog.
	 */
	void UpdateDialogValues();
	/**
	 * Updates the controsl with current visibility configuration.
	 */
	void UpdateVisivilityControls();
	/**
	 * @brief Add new type menu control to the dialog.
	 * @param name Name of the control.same as driver nameof the control.
	 * @param val Control structure.
	 * @param pos Position of the control, number of the control.
	 */
	void AddControlMenu(string name, controlstr val, int pos);
	/**
	 * @brief Add new type int control to the dialog, with editbox and slider.
	 * @param name Name of the control.same as driver nameof the control.
	 * @param val Control structure.
	 * @param pos Position of the control, number of the control.
	 */
	void AddControlInt(string name, controlstr val,int pos);
	/**
	 * @brief Add new type checkbox control to the dialog.
	 * @param name Name of the control.same as driver nameof the control.
	 * @param val Control structure.
	 * @param pos Position of the control, number of the control.
	 */
	void AddControlCheckbox(string name, controlstr val,int pos);
	/**
	 * @brief clear all controls.
	 */
	void ClearControls();
	/**
	 * @brief Update image in the visor if executing as aplication.
	 * @param img New image
	 */
	void UpdateImage(v4l2image *img);
	/**
	 * @brief YUV422toRGB888 YUV422 to  RGB888 converter.
	 * @param width Image width.
	 * @param height Image height.
	 * @param src	Source raw buffer.
	 * @param dst Dest raw buffer.
	 */
 void YUV422toRGB888(int width, int height, unsigned char *src, unsigned char *dst);
	 /**
		* @brief Add new message to status bar.
		* @param msg New message.
		*/
	 void AddMessage(string msg);
	 /**
		* @brief Show the aux image viewer
		* @param visible true for show the viewer.
		*/
	 void ShowViewer(bool visible){visible ? m_visor->show(): m_visor->hide();};
public slots:
	 /**
		* @brief One second timmer.
		*/
	void OnTimer1s();
	/**
	 * @brief Slot from menu type parameter changed.
	 * @param controlname Name of the control.
	 * @return
	 */
	int AplyConfigCombo(QString controlname);
	/**
	 * @brief Slot from checkbox type parameter changed.
	 * @param controlname Name of the control.
	 * @return
	 */
	int AplyConfigCheckbox(QString controlname);
	/**
	 * @brief Slot from int type parameter changed(slider).
	 * @param controlname Name of the control.
	 * @return
	 */
	int AplyConfigSlider(QString controlname);
	/**
	 * @brief Slot from int type parameter changed (spinbox).
	 * @param controlname Name of the control.
	 * @return
	 */
	int AplyConfigDSpinbox(QString controlname);
	/**
	 * @brief Slot from adquisition thread when working as main program.
	 * @param controlname Name of the control.
	 * @return
	 */
	int OnNewImage(v4l2image *img);
private slots:
	void on_pushButton_open_device_clicked();
	void on_pushButton_update_devicelist_clicked();
	void on_comboBox_device_list_currentIndexChanged(int);
	void on_pushButton_set_resolution_clicked();
	void on_pushButton_start_clicked();
signals:
	void SGNewImage(v4l2image *img);
private:
	/**
	 * @brief Thread to adquire when working as main application.
	 * @param param this pointer.
	 * @return
	 */
	static void* ThreadAdq(void* param);
public:
	int m_curcam;																	//!< Current class number
	int m_stop;																		//!< Adquisition thread status, 0 run 1 stop request -1 stoped.
	static pthread_t m_pthreadhiloadq[MAX_CAMS];	//!< Adquisition threads pointers
private:
	Ui::v4lcamera *ui;
	static int m_ncams;														//!< Number of instances of the class(number of cams)
	bool m_islibrary;															//!< true if work as acquisition library 0 if work as main program (visor enabled)
	QVector <pair<QString,QPushButton*> > m_vectcontrolbuttons;				//!< Dinamic buttons vector.
	QVector <QLabel*> m_vectcontrollabels;														//!< Dinamic labesl vector.
	QVector <pair<QString,QComboBox*> > m_vectcontrolcombos;					//!< Dinamic combobox vector.
	QVector <pair<QString,QSlider*> > m_vectcontrolsliders;						//!< Dinamic sliders vector.
	QVector <pair<QString,QDoubleSpinBox*> > m_vectcontroldspinbox;		//!< Dinamic spinbox vector.
	QVector <pair<QString,QCheckBox*> > m_vectcontrolcheckbox;				//!< Dinamic checkbox vector.
	QMutex m_controlmutex;										//!< Mutex for creating/deleting controls.
	QSignalMapper *m_sigmappercombos;						//!< Signal mapper for combobox(menu type parameter)
	QSignalMapper *m_sigmappercchecbx;					//!< Signal mapper for checkbox
	QSignalMapper *m_sigmappersliders;					//!< Signal mapper for sliders (int type parameter)
	QSignalMapper *m_sigmappersspinbox;					//!< Signal mapper for spinbox (int type parameter)
	QTimer m_timer1s;														//!< 1 second timer.
	/// IMAGE VISOR DIALOG
	QWidget *m_visor;														//!< Image visor widget when working as main application.
	QGridLayout *m_layoutvisor;									//!< Layout for image visor.
	QLabel m_framevisor;												//!< Qlabel for image painting.

};

#endif // V4LCAMERA_H
