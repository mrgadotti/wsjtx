// -*- Mode: C++ -*-
#ifndef VERTICALWATERFALL_H_
#define VERTICALWATERFALL_H_

#include <QDialog>
#include <QScopedPointer>
#include <QDir>
#include "WFPalette.hpp"
#include "commons.h"          // NSMAX

// Deliberately NOT named MAX_SCREENSIZE: widegraph.h already defines that
// macro (2048) and verticalwaterfall.h ends up included (indirectly) from
// widegraph.cpp. Using a distinct constexpr avoids any clash.
constexpr int MAX_VERT_BINS = 2048;

namespace Ui {
  class VerticalWaterfall;
}

class QSettings;

class VerticalWaterfall : public QDialog
{
  Q_OBJECT

public:
  explicit VerticalWaterfall(QSettings *, QWidget *parent = 0);
  ~VerticalWaterfall ();

  void   dataSink2(float s[], float df3, int ihsym, int ndiskdata);
  void   setRxFreq(int n);
  void   setTxFreq(int n);
  void   setPeriod(double trperiod, int nsps);
  void   setMode(QString mode);
  void   setSubMode(int n);
  void   setTol(int n);
  void   setSuperFox(bool b);
  void   setSuperHound(bool b);
  void   setRxBand (QString const& band);
  void   setWSPRtransmitted();
  void   setVHF(bool bVHF);
  void   setFST4_FreqRange(int fLow,int fHigh);
  void   setSingleDecode(bool b);
  void   setDiskUTC(int nutc);
  void   setDarkStyle(bool b);
  void   setDialFreq(double d);
  void   saveSettings();
  void   forceClose();            // actually close (app shutdown), unlike closeEvent's normal ignore()

signals:
  void freezeDecode2(int n);
  void f11f12(int n);
  void setFreq3(int rxFreq, int txFreq);

public slots:
  void wideFreezeDecode(int n);
  void setFreq2(int rxFreq, int txFreq);

protected:
  void keyPressEvent (QKeyEvent *e) override;
  void closeEvent (QCloseEvent *) override;

private slots:
  void on_waterfallAvgSpinBox_valueChanged(int arg1);
  void on_bppSpinBox_valueChanged(int arg1);
  void on_fStartSpinBox_valueChanged(int n);
  void on_paletteComboBox_activated(const QString &palette);
  void on_timestampComboBox_currentIndexChanged(int n);
  void on_cbFlatten_toggled(bool b);
  void on_cbControls_toggled(bool b);
  void on_cbBars_toggled(bool b);
  void on_cbFreq_toggled(bool b);
  void on_adjust_palette_push_button_clicked (bool);
  void on_gainSlider_valueChanged(int value);
  void on_zeroSlider_valueChanged(int value);

private:
  void readPalette ();
  void replot();

  QScopedPointer<Ui::VerticalWaterfall> ui;

  QSettings * m_settings;
  QDir m_palettes_path;
  WFPalette m_userPalette;

  double m_tr0;
  double m_TRperiod;

  qint32 m_waterfallAvg;
  qint32 m_nsps;
  qint32 m_n;
  qint32 m_timestamp;

  bool	 m_bars;
  bool	 m_freq;
  bool   m_bFlatten;
  bool   m_bHaveTransmitted;    //Set true at end of a WSPR or FT4 transmission
  bool   m_shuttingDown = false; // true only while the application itself is exiting

  QString m_rxBand;
  QString m_mode;
  QString m_waterfallPalette;
  float   m_swide[MAX_VERT_BINS];
  float   m_splot[NSMAX];
  QString m_user_defined;
};

#endif // VERTICALWATERFALL_H_
