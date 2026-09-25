#include "verticalplotter.h"
#include "DriftingDateTime.hpp"
#include <math.h>
#include <cstring>
#include <QPainter>
#include <QDateTime>
#include <QTime>
#include <QPen>
#include <QMouseEvent>
#include "qt_helpers.hpp"
#include "moc_verticalplotter.cpp"

extern "C" {
  void flat4_(float swide[], int* iz, int* nflatten);
}

CVerticalPlotter::CVerticalPlotter(QWidget *parent) :          //CVerticalPlotter Constructor
  QFrame {parent},
  m_bScaleOK {false},
  m_bVHF {false},
  m_bSingleDecode {false},
  m_bSuperFox {false},
  m_bSuperHound {false},
  m_bars {false},
  m_freq {false},
  m_useDarkStyle {false},
  m_paintEventBusy {false},
  m_bReplot {false},
  m_plotZero {0},
  m_plotGain {0},
  m_binsPerPixel {2},
  m_waterfallAvg {1},
  m_Flatten {1},
  m_nSubMode {0},
  m_nfa {0},
  m_nfb {0},
  m_nUTC {-1},
  m_timestamp {0},
  m_fSpan {2000.0},
  m_fftBinWidth {1500.0/2048.0},
  m_dialFreq {0.},
  m_xOffset {0.},
  m_TRperiod {0.},
  m_freqPerDiv {10},
  m_hdivs {0},
  m_nsps {6912},
  m_w {0},
  m_h {0},
  m_wScale {58},
  m_wf {0},
  m_vMargin {12},
  m_rxFreq {1020},
  m_txFreq {0},
  m_fMax {0},
  m_startFreq {0},
  m_tol {100},
  m_lastMouseY {-1},
  m_line {0}
{
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
  setFocusPolicy(Qt::StrongFocus);
  setAttribute(Qt::WA_PaintOnScreen,false);
  setAutoFillBackground(false);
  setAttribute(Qt::WA_OpaquePaintEvent, false);
  setAttribute(Qt::WA_NoSystemBackground, true);
  setMouseTracking(true);
  m_colours = QVector<QColor> (256, Qt::black);       // safe default until setColours() runs
}

CVerticalPlotter::~CVerticalPlotter() { }                      // Destructor

QSize CVerticalPlotter::minimumSizeHint() const
{
  return QSize(150, 200);
}

QSize CVerticalPlotter::sizeHint() const
{
  return QSize(220, 400);
}

void CVerticalPlotter::resizeEvent(QResizeEvent* )             //resizeEvent()
{
  if(!size().isValid()) return;
  if(m_Size != size()) {
    m_Size = size();
    m_w = m_Size.width();
    m_h = m_Size.height();
    m_wf = m_w - m_wScale;
    if(m_wf < 1) m_wf = 1;
    m_WaterfallPixmap = QPixmap(m_wf, m_h);
    m_WaterfallPixmap.fill(Qt::black);
    m_ScalePixmap = QPixmap(m_wScale, m_h);
    m_ScalePixmap.fill(Qt::white);
    DrawOverlay();
    replot();                           //rebuild the waterfall pixmap from history at the new size
  } else {
    DrawOverlay();
  }
}

void CVerticalPlotter::paintEvent(QPaintEvent *)                //paintEvent()
{
  if(m_paintEventBusy) return;
  m_paintEventBusy=true;
  QPainter painter(this);
  painter.drawPixmap(0,0,m_ScalePixmap);
  painter.drawPixmap(m_wScale,0,m_WaterfallPixmap);

  if(m_bars) {
    float bw = modeBandwidth();
    bool rxOk = m_mode.startsWith("FT") or m_mode.startsWith("JT")
                or m_mode=="Q65" or m_mode.startsWith("FST4");
    bool txOk = rxOk or m_mode.startsWith("WSPR");

    if(rxOk) {
      float rxBw = bw;
      if(m_mode=="FT8" and m_bSuperHound) rxBw=1500.0;
      painter.setPen(Qt::green);
      int y1=YfromFreq(m_rxFreq);
      int y2=YfromFreq(m_rxFreq+rxBw);
      painter.drawLine(m_wScale,y1,m_w,y1);
      painter.drawLine(m_wScale,y2,m_w,y2);
    }

    if(txOk) {
      float txBw=bw;
      float txF0=m_txFreq;
      if(m_mode=="FT8" and m_bSuperFox) txBw=1500.0;
      if(m_mode=="WSPR") {
        txBw=4.0*12000.0/8192.0;
        txF0=m_txFreq-0.5*txBw;
      }
      painter.setPen(Qt::red);
      int y1=YfromFreq(txF0);
      int y2=YfromFreq(txF0+txBw);
      painter.drawLine(m_wScale,y1,m_w,y1);
      painter.drawLine(m_wScale,y2,m_w,y2);
    }

    if(m_lastMouseY >= 0) {
      int dy = YfromFreq(m_rxFreq) - YfromFreq(m_rxFreq+bw);
      painter.setPen(Qt::white);
      painter.drawLine(m_wScale,m_lastMouseY,m_w,m_lastMouseY);
      painter.drawLine(m_wScale,m_lastMouseY-dy,m_w,m_lastMouseY-dy);
    }
  }
  m_paintEventBusy=false;
}

void CVerticalPlotter::draw(float swide[], bool bScroll)
{
  if(!m_TRperiod) return;               // not ready to plot yet
  double fac = sqrt(m_binsPerPixel*m_waterfallAvg/15.0);
  double gain = fac*pow(10.0,0.015*m_plotGain);

  int iz = freqPixels();
  if(iz <= 0) return;

  if(bScroll and swide[0]<1.e29) {
    flat4_(swide,&iz,&m_Flatten);       // flatten/convert to dB, on the local buffer only
  }

  if(bScroll and !m_bReplot) {
    m_WaterfallPixmap.scroll(-1,0,0,0,m_wf,m_h);   //move current data left
    pushHistory(swide, iz);
  }

  QPainter painter1(&m_WaterfallPixmap);
  drawColumn(painter1, swide, iz, m_wf-1, gain);   //new column enters on the right

  m_line++;
  if(swide[0]>1.0e29) m_line=0;
  if(m_mode=="FT4" and m_line==34) m_line=0;
  if(m_line == painter1.fontMetrics().height() and m_timestamp!=0 and !m_bReplot) {
    painter1.setPen(Qt::white);
    QString t;
    if(m_nUTC<0) {
      auto start = qt_truncate_date_time_to (DriftingDateTime::currentDateTimeUtc(), m_TRperiod * 1e3)
        .toString (m_TRperiod < 60. ? "hh:mm:ss" : "hh:mm");
      t = QString {"%1    %2"}.arg (start, m_rxBand);
    } else {
      auto hr = m_nUTC / 10000;
      auto start = QTime {hr, (m_nUTC - 10000 * hr) / 100, m_nUTC % 100}
         .toString (m_TRperiod < 60. ? "hh:mm:ss" : "hh:mm");
      t = QString {"%1    %2"}.arg (start).arg (m_rxBand);
    }
    painter1.save();
    painter1.translate(m_wf - m_line, m_h);
    painter1.rotate(-90);
    QRect rect{5, 0, m_h - 10, painter1.fontMetrics().height()};
    painter1.drawText(rect, m_timestamp==2 ? Qt::AlignRight|Qt::AlignVCenter
                                            : Qt::AlignLeft|Qt::AlignVCenter, t);
    painter1.restore();
  }

  update();                             //trigger a new paintEvent
  m_bScaleOK=true;
}

void CVerticalPlotter::drawColumn(QPainter& p, const float* swide, int iz, int x, double gain)
{
  if(swide[0]>1.e29 and swide[0]<1.5e30) p.setPen(Qt::green);   //end of an Rx interval
  if(swide[0]>1.4e30)                    p.setPen(Qt::red);     //ditto, after a Tx interval
  for(int i=0; i<iz; i++) {
    int y1 = 10.0*gain*swide[i] + m_plotZero;
    if(y1<0) y1=0;
    if(y1>254) y1=254;
    if(swide[i]<1.e29) p.setPen(m_colours[y1]);
    p.drawPoint(x, m_h-1-m_vMargin-i);  //i is the frequency bin; bin 0 sits just above the bottom margin
  }
}

void CVerticalPlotter::pushHistory(const float* swide, int iz)
{
  QVector<float> col(iz);
  memcpy(col.data(), swide, iz*sizeof(float));
  m_history.append(col);
  while(m_history.size() > m_wf) m_history.removeFirst();
}

void CVerticalPlotter::replot()
{
  if(m_WaterfallPixmap.isNull()) return;
  m_WaterfallPixmap.fill(Qt::black);
  m_bReplot=true;
  double fac = sqrt(m_binsPerPixel*m_waterfallAvg/15.0);
  double gain = fac*pow(10.0,0.015*m_plotGain);
  QPainter painter1(&m_WaterfallPixmap);
  int n = m_history.size();
  for(int k=0; k<n; k++) {
    drawColumn(painter1, m_history[k].constData(), m_history[k].size(), m_wf-n+k, gain);
  }
  m_bReplot=false;
  update();
}

float CVerticalPlotter::modeBandwidth() const
{
  float bw=9.0*12000.0/m_nsps;               //JT9
  if(m_mode=="FT4") bw=3*12000.0/576.0;      //FT4  ### (3x, or 4x???) ###
  if(m_mode=="FT8") bw=7*12000.0/1920.0;     //FT8
  if(m_mode.startsWith("FST4")) {
    int h=int(pow(2.0,m_nSubMode));
    int nsps=800;
    if(m_TRperiod==30) nsps=1680;
    if(m_TRperiod==60) nsps=4000;
    if(m_TRperiod==120) nsps=8400;
    if(m_TRperiod==300) nsps=21504;
    if(m_TRperiod==900) nsps=66560;
    if(m_TRperiod==1800) nsps=134400;
    float baud=12000.0/nsps;
    bw=3.0*h*baud;
  }
  if(m_mode=="JT4") {                        //JT4
    bw=3*11025.0/2520.0;                     //Max tone spacing (3/4 of actual BW)
    if(m_nSubMode==1) bw=2*bw;
    if(m_nSubMode==2) bw=4*bw;
    if(m_nSubMode==3) bw=9*bw;
    if(m_nSubMode==4) bw=18*bw;
    if(m_nSubMode==5) bw=36*bw;
    if(m_nSubMode==6) bw=72*bw;
  }
  if(m_mode=="JT9" and m_nSubMode>0) {       //JT9
    bw=8.0*12000.0/m_nsps;
    if(m_nSubMode==1) bw=2*bw;   //B
    if(m_nSubMode==2) bw=4*bw;   //C
    if(m_nSubMode==3) bw=8*bw;   //D
    if(m_nSubMode==4) bw=16*bw;  //E
    if(m_nSubMode==5) bw=32*bw;  //F
    if(m_nSubMode==6) bw=64*bw;  //G
    if(m_nSubMode==7) bw=128*bw; //H
  }
  if(m_mode=="Q65") {                        //Q65
    int h=int(pow(2.0,m_nSubMode));
    int nsps=1800;
    if(m_TRperiod==30) nsps=3600;
    if(m_TRperiod==60) nsps=7200;
    if(m_TRperiod==120) nsps=16000;
    if(m_TRperiod==300) nsps=41472;
    float baud=12000.0/nsps;
    bw=65.0*h*baud;
  }
  if(m_mode=="JT65") {                       //JT65
    bw=65.0*11025.0/4096.0;
    if(m_nSubMode==1) bw=2*bw;   //B
    if(m_nSubMode==2) bw=4*bw;   //C
  }
  return bw;
}

void CVerticalPlotter::drawGoalPost(QPainter& p, QPen const& pen, float f0, float bw, int xDepth, int xh)
{
  int y1 = YfromFreq(f0);
  int y2 = YfromFreq(f0+bw);
  p.setPen(pen);
  p.drawLine(xDepth,y1,xDepth,y2);            // vertical segment between f0 and f0+bw
  p.drawLine(xDepth-xh,y1,xDepth,y1);         // ear at f0
  p.drawLine(xDepth-xh,y2,xDepth,y2);         // ear at f0+bw
}

void CVerticalPlotter::DrawOverlay()                            //DrawOverlay()
{
  if(m_ScalePixmap.isNull()) return;
  if(m_WaterfallPixmap.isNull()) return;

  double df = m_binsPerPixel*m_fftBinWidth;
  QPen penOrange(QColor(255,165,0),3);
  QPen penGreen(QColor(15,153,105), 3);        //Mark Tol range or BW with dark green line
  if(m_useDarkStyle) penGreen.setBrush(Qt::green);   // lighter green for dark style
  QPen penRed(Qt::red, 3);                     //Mark Tx freq with red

  m_fSpan = (m_h - 2*m_vMargin)*df;   // leave room top/bottom so labels aren't clipped
  m_freqPerDiv=10;
  if(m_fSpan>100) m_freqPerDiv=20;
  if(m_fSpan>250) m_freqPerDiv=50;
  if(m_fSpan>500) m_freqPerDiv=100;
  if(m_fSpan>1000) m_freqPerDiv=200;
  if(m_fSpan>2500) m_freqPerDiv=500;

  if(m_binsPerPixel < 1) m_binsPerPixel=1;
  m_hdivs = int(m_h*df/m_freqPerDiv + 0.9999);
  if(m_hdivs > 480) m_hdivs=480;
  if(m_hdivs < 0) m_hdivs=0;

  QPainter painter0(&m_ScalePixmap);
  painter0.setBackground (palette ().brush (backgroundRole ()));

  QFont Font("Arial");
  Font.setPointSize(9);
  Font.setWeight(QFont::Normal);
  painter0.setFont(Font);

  if (m_useDarkStyle) {
    m_ScalePixmap.fill(Qt::black);
    painter0.setPen(Qt::white);
  } else {
    m_ScalePixmap.fill(Qt::white);
    painter0.setPen(Qt::black);
  }
  painter0.drawRect(0, 0, m_wScale-1, m_h-1);

  MakeFrequencyStrs();

  int minor=5;
  if(m_freqPerDiv==200) minor=4;

  for(int i=0; i<=m_hdivs; i++) {               //major ticks + frequency labels
    int f = m_HDivText[i].toInt();
    int y = YfromFreq(f);
    painter0.drawLine(34,y,40,y);
    QRect rect0(0, y-10, 33, 20);
    painter0.drawText(rect0, Qt::AlignRight|Qt::AlignVCenter, m_HDivText[i]);
  }

  double minorStep = double(m_freqPerDiv)/minor;
  for(int i=1; i*minorStep <= m_fSpan; i++) {  //minor ticks
    float f = m_startFreq + i*minorStep;
    int y = YfromFreq(f);
    painter0.drawLine(36,y,40,y);
  }

  float bw = modeBandwidth();

  bool rxOk = m_mode.startsWith("FT") or m_mode.startsWith("JT")
              or m_mode=="Q65" or m_mode.startsWith("FST4");
  bool txOk = rxOk or m_mode.startsWith("WSPR");

  bool tolOk = m_mode=="Q65" or (m_mode=="JT65" and m_bVHF)
               or (m_mode=="FT8" and m_bSuperHound)
               or m_mode.startsWith("FST4") or m_mode=="FreqCal";
  if(tolOk) {
    int y1=YfromFreq(m_rxFreq-m_tol);
    int y2=YfromFreq(m_rxFreq+m_tol);
    painter0.setPen(penGreen);
    painter0.drawLine(51,y1,51,y2);
  }

  if(m_mode=="WSPR") {
    int y1=YfromFreq(1400);
    int y2=YfromFreq(1600);
    painter0.setPen(penGreen);
    painter0.drawLine(51,y1,51,y2);
  }

  if(rxOk) {
    float rxBw = bw;
    if(m_mode=="FT8" and m_bSuperHound) rxBw=1500.0;
    drawGoalPost(painter0, penGreen, m_rxFreq, rxBw, 55, 6);
  }

  if(txOk) {
    float txBw=bw;
    float txF0=m_txFreq;
    if(m_mode=="FT8" and m_bSuperFox) txBw=1500.0;
    if(m_mode=="WSPR") {
      txBw=4.0*12000.0/8192.0;
      txF0=m_txFreq-0.5*txBw;
    }
    drawGoalPost(painter0, penRed, txF0, txBw, 47, 5);
  }

  if(m_mode=="FST4" and !m_bSingleDecode) {    // Mark FST4 F_Low / F_High
    int y1=YfromFreq(m_nfa);
    int y2=YfromFreq(m_nfb);
    painter0.setPen(penGreen);
    painter0.drawLine(37,y1,40,y1+5);
    painter0.drawLine(37,y1,34,y1+5);
    painter0.drawLine(37,y2,40,y2-5);
    painter0.drawLine(37,y2,34,y2-5);
  }

  if(m_dialFreq>10.13 and m_dialFreq<10.15 and m_mode.mid(0,4)!="WSPR" and m_mode!="FST4W") {
    float f1=1.0e6*(10.1401 - m_dialFreq);
    float f2=f1+200.0;
    int y1=YfromFreq(f1);
    int y2=YfromFreq(f2);
    if(y1>=0 or y2>=0) {
      painter0.setPen(penOrange);             //Mark WSPR sub-band orange
      painter0.drawLine(44,y1,44,y2);
    }
  }

  updateFMax();
}

void CVerticalPlotter::MakeFrequencyStrs()                      //MakeFrequencyStrs
{
  int f=(m_startFreq+m_freqPerDiv-1)/m_freqPerDiv;
  f*=m_freqPerDiv;
  m_xOffset=float(f-m_startFreq)/m_freqPerDiv;
  for(int i=0; i<=m_hdivs; i++) {
    m_HDivText[i].setNum(f);
    f+=m_freqPerDiv;
  }
}

int CVerticalPlotter::YfromFreq(float f)                         //YfromFreq()
{
  int ph = m_h - 2*m_vMargin;
  if(ph < 1) ph = 1;
  int y = m_h - 1 - m_vMargin - int(ph * (f - m_startFreq)/m_fSpan + 0.5);
  if(y<m_vMargin) return m_vMargin;
  if(y>m_h-1-m_vMargin) return m_h-1-m_vMargin;
  return y;
}

float CVerticalPlotter::FreqfromY(int y)                         //FreqfromY()
{
  return float(m_startFreq + (m_h - 1 - m_vMargin - y)*m_binsPerPixel*m_fftBinWidth);
}

int CVerticalPlotter::freqPixels()                               //freqPixels()
{
  int n = int((5000.0 - m_startFreq)/(m_binsPerPixel*m_fftBinWidth));
  return qBound(0, n, m_h - 2*m_vMargin);
}

void CVerticalPlotter::updateFMax()
{
  m_fMax = m_startFreq + freqPixels()*m_binsPerPixel*m_fftBinWidth;
}

void CVerticalPlotter::setPlotZero(int plotZero)                //setPlotZero()
{
  m_plotZero=plotZero;
}

int CVerticalPlotter::plotZero()                                //plotZero()
{
  return m_plotZero;
}

void CVerticalPlotter::setPlotGain(int plotGain)                //setPlotGain()
{
  m_plotGain=plotGain;
}

int CVerticalPlotter::plotGain()                                //plotGain()
{
  return m_plotGain;
}

void CVerticalPlotter::setStartFreq(int f)                      //setStartFreq()
{
  m_startFreq=f;
  DrawOverlay();
  update();
}

int CVerticalPlotter::startFreq()                                //startFreq()
{
  return m_startFreq;
}

int CVerticalPlotter::plotHeight()
{
  return m_WaterfallPixmap.height();
}

void CVerticalPlotter::setBinsPerPixel(int n)                   //setBinsPerPixel
{
  m_binsPerPixel = n;
  DrawOverlay();                          //Redraw scale and ticks
  update();
}

int CVerticalPlotter::binsPerPixel()                              //binsPerPixel
{
  return m_binsPerPixel;
}

void CVerticalPlotter::setWaterfallAvg(int n)                    //setWaterfallAvg
{
  m_waterfallAvg = n;
}

void CVerticalPlotter::setRxFreq(int n)                           //setRxFreq
{
  m_rxFreq = n;
  DrawOverlay();
  update();
}

int CVerticalPlotter::rxFreq()
{
  return m_rxFreq;
}

void CVerticalPlotter::leaveEvent(QEvent *event)
{
  m_lastMouseY = -1;
  event->ignore();
}

void CVerticalPlotter::mouseMoveEvent (QMouseEvent * event)
{
  int y=event->y();
  if(y < m_vMargin) y = m_vMargin;
  if(y > m_h-1-m_vMargin) y = m_h-1-m_vMargin;
  m_lastMouseY = y;
  update();

  event->ignore();
  if(m_freq) {
    QToolTip::showText(event->globalPos(),QString::number(int(FreqfromY(y))));
  }
  QWidget::mouseMoveEvent(event);
}

void CVerticalPlotter::mouseReleaseEvent (QMouseEvent * event)
{
  bool rightbutton = (event->button() & Qt::RightButton);
  bool leftbutton = (event->button() & Qt::LeftButton);
  bool shift = (event->modifiers() & Qt::ShiftModifier);
  if (rightbutton) {
     leftbutton = true;
     shift = true;
  }
  if (leftbutton) {
    int y=event->y();
    if(y<m_vMargin) y=m_vMargin;
    if(y>m_h-1-m_vMargin) y=m_h-1-m_vMargin;
    bool ctrl = (event->modifiers() & Qt::ControlModifier);
    if(!shift and m_mode=="FST4W") return;
    int newFreq = int(FreqfromY(y)+0.5);
    int oldTxFreq = m_txFreq;
    int oldRxFreq = m_rxFreq;
    if (ctrl and m_mode!="FST4W") {
      emit setFreq1 (newFreq, newFreq);
    } else if (shift) {
      emit setFreq1 (oldRxFreq, newFreq);
    } else {
      emit setFreq1(newFreq,oldTxFreq);
    }

    int n=1;
    if(ctrl) n+=100;
    emit freezeDecode1(n);
  }
  else {
    event->ignore ();           // let parent handle
  }
}

void CVerticalPlotter::mouseDoubleClickEvent (QMouseEvent * event)
{
  bool rightbutton = (event->button() & Qt::RightButton);
  bool leftbutton = (event->button() & Qt::LeftButton);
  bool ctrl = (event->modifiers() & Qt::ControlModifier);
  if (leftbutton) {
    int n=2;
    if(ctrl) n+=100;
    emit freezeDecode1(n);
  } else if (rightbutton) {
    int y=event->y();
    if(y<m_vMargin) y=m_vMargin;
    if(y>m_h-1-m_vMargin) y=m_h-1-m_vMargin;
    int newFreq = int(FreqfromY(y)+0.5);
    if (m_mode!="FST4W") emit setFreq1 (newFreq, newFreq);
    int n=1;
    if(ctrl) n+=100;
    emit freezeDecode1(n);
  } else {
    event->ignore ();           // let parent handle
  }
}

void CVerticalPlotter::setNsps(double trperiod, int nsps)         //setNsps
{
  m_TRperiod=trperiod;
  m_nsps=nsps;
  m_fftBinWidth=1500.0/2048.0;
  if(m_nsps==15360)  m_fftBinWidth=1500.0/2048.0;
  if(m_nsps==40960)  m_fftBinWidth=1500.0/6144.0;
  if(m_nsps==82944)  m_fftBinWidth=1500.0/12288.0;
  if(m_nsps==252000) m_fftBinWidth=1500.0/32768.0;
  DrawOverlay();                          //Redraw scale and ticks
  update();
}

void CVerticalPlotter::setTxFreq(int n)                            //setTxFreq
{
  m_txFreq=n;
  DrawOverlay();
  update();
}

void CVerticalPlotter::setMode(QString mode)                       //setMode
{
  m_mode=mode;
}

void CVerticalPlotter::setSubMode(int n)                            //setSubMode
{
  m_nSubMode=n;
}

int CVerticalPlotter::Fmax()
{
  return m_fMax;
}

void CVerticalPlotter::setDialFreq(double d)
{
  m_dialFreq=d;
  DrawOverlay();
  update();
}

void CVerticalPlotter::setRxBand(QString band)
{
  m_rxBand=band;
}

void CVerticalPlotter::setFlatten(bool b)
{
  m_Flatten = b ? 1 : 0;
}

void CVerticalPlotter::setSuperFox(bool b)
{
  m_bSuperFox=b;
  if(m_bSuperFox) m_bSuperHound=false;
}

void CVerticalPlotter::setSuperHound(bool b)
{
  m_bSuperHound=b;
  if(m_bSuperHound) m_bSuperFox=false;
}

void CVerticalPlotter::setTol(int n)                                //setTol()
{
  m_tol=n;
  DrawOverlay();
}

void CVerticalPlotter::setFST4_FreqRange(int fLow,int fHigh)
{
  m_nfa=fLow;
  m_nfb=fHigh;
  DrawOverlay();
  update();
}

void CVerticalPlotter::setSingleDecode(bool b)
{
  m_bSingleDecode=b;
}

void CVerticalPlotter::setColours(QVector<QColor> const& cl)
{
  m_colours = cl;
}

void CVerticalPlotter::setVHF(bool bVHF)
{
  m_bVHF=bVHF;
}

void CVerticalPlotter::setDiskUTC(int nutc)
{
  m_nUTC=nutc;
}

void CVerticalPlotter::setTimestamp(int n)
{
  m_timestamp=n;
}

void CVerticalPlotter::setBars(bool b)
{
  setMouseTracking(b || m_freq);
  m_bars=b;
  DrawOverlay();
  update();
}

void CVerticalPlotter::showFreq(bool b)
{
  setMouseTracking(b || m_bars);
  m_freq=b;
}

void CVerticalPlotter::setDarkStyle (bool b)
{
  m_useDarkStyle=b;
  DrawOverlay();
  update();
}
