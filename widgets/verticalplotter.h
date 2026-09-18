// -*- Mode: C++ -*-
///////////////////////////////////////////////////////////////////////////
// CVerticalPlotter is a rotated, waterfall-only sibling of CPlotter
// (see widgets/plotter.h). Frequency runs on the vertical axis (low at
// the bottom, high at the top) and time runs on the horizontal axis,
// with the newest column entering at the right and history scrolling to
// the left. There is no 2D spectrum plot here, only the waterfall and
// its frequency scale.
///////////////////////////////////////////////////////////////////////////

#ifndef VERTICALPLOTTER_H_
#define VERTICALPLOTTER_H_

#include <QFrame>
#include <QSize>
#include <QVector>
#include <QColor>
#include <QToolTip>

class QPainter;
class QPen;

class CVerticalPlotter : public QFrame
{
  Q_OBJECT

public:
  explicit CVerticalPlotter(QWidget *parent = 0);
  ~CVerticalPlotter();

  QSize minimumSizeHint() const Q_DECL_OVERRIDE;
  QSize sizeHint() const Q_DECL_OVERRIDE;

  void draw(float swide[], bool bScroll);              //Update the waterfall
  void replot();
  void setPlotZero(int plotZero);
  int  plotZero();
  void setPlotGain(int plotGain);
  int  plotGain();
  void setStartFreq(int f);
  int  startFreq();
  int  plotHeight();
  void setBinsPerPixel(int n);
  int  binsPerPixel();
  void setWaterfallAvg(int n);
  void setRxFreq(int n);
  int  rxFreq();
  void setNsps(double trperiod, int nsps);
  void setTxFreq(int n);
  void setMode(QString mode);
  void setSubMode(int n);
  int  Fmax();
  void setDialFreq(double d);
  float fSpan() const {return m_fSpan;}
  void setColours(QVector<QColor> const& cl);
  void setTimestamp(int n);
  void setBars(bool b);
  void showFreq(bool b);
  void setFlatten(bool b);
  void setTol(int n);
  void setSuperFox(bool b);
  void setSuperHound(bool b);
  void setRxBand(QString band);
  void setVHF(bool bVHF);
  void setFST4_FreqRange(int fLow,int fHigh);
  void setSingleDecode(bool b);
  void setDiskUTC(int nutc);
  bool scaleOK () const {return m_bScaleOK;}
  void setDarkStyle(bool b);
  void DrawOverlay();

signals:
  void freezeDecode1(int n);
  void setFreq1(int rxFreq, int txFreq);

protected:
  //re-implemented widget event handlers
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent* event) override;
  void mouseMoveEvent(QMouseEvent * event) override;
  void mouseReleaseEvent (QMouseEvent * event) override;
  void mouseDoubleClickEvent (QMouseEvent * event) override;

private:
  void MakeFrequencyStrs();
  int   YfromFreq(float f);
  float FreqfromY(int y);
  int   freqPixels();
  void  updateFMax();
  float modeBandwidth() const;
  void  drawColumn(QPainter& painter, const float* swide, int iz, int x, double gain);
  void  drawGoalPost(QPainter& painter, QPen const& pen, float f0, float bw, int xDepth, int xh);
  void  pushHistory(const float* swide, int iz);

  bool    m_bScaleOK;
  bool    m_bVHF;
  bool    m_bSingleDecode;
  bool    m_bSuperFox;
  bool    m_bSuperHound;
  bool    m_bars;
  bool    m_freq;
  bool    m_useDarkStyle;

  bool    m_paintEventBusy;
  bool    m_bReplot;

  qint32  m_plotZero;
  qint32  m_plotGain;
  qint32  m_binsPerPixel;
  qint32  m_waterfallAvg;
  qint32  m_Flatten;
  qint32  m_nSubMode;
  qint32  m_nfa;
  qint32  m_nfb;
  qint32  m_nUTC;
  qint32  m_timestamp;

  float   m_fSpan;
  double  m_fftBinWidth;
  double  m_dialFreq;
  double  m_xOffset;
  double  m_TRperiod;

  qint32  m_freqPerDiv;
  qint32  m_hdivs;
  qint32  m_nsps;
  qint32  m_w;
  qint32  m_h;
  qint32  m_wScale;
  qint32  m_wf;
  qint32  m_vMargin;    // blank rows reserved at top/bottom so scale labels aren't clipped
  qint32  m_rxFreq;
  qint32  m_txFreq;
  qint32  m_fMax;
  qint32  m_startFreq;
  qint32  m_tol;
  qint32  m_lastMouseY;
  qint32  m_line;

  QPixmap m_WaterfallPixmap;
  QPixmap m_ScalePixmap;
  QSize   m_Size;
  QString m_HDivText[483];
  QString m_mode;
  QString m_rxBand;

  QVector<QColor> m_colours;
  QVector<QVector<float>> m_history;   // per-column dB data, newest at the end

private slots:
  void leaveEvent(QEvent *event) override;
};

#endif // VERTICALPLOTTER_H_
