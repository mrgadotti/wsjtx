#include "verticalwaterfall.h"
#include "DriftingDateTime.hpp"

#include <algorithm>
#include <QApplication>
#include <QSettings>
#include <QDateTime>
#include <QKeyEvent>
#include <QCloseEvent>
#include <math.h>
#include "ui_verticalwaterfall.h"
#include "MessageBox.hpp"
#include "SettingsGroup.hpp"
#include "moc_verticalwaterfall.cpp"

VerticalWaterfall::VerticalWaterfall(QSettings * settings, QWidget *parent) :
  QDialog(parent),
  ui(new Ui::VerticalWaterfall),
  m_settings (settings),
  m_palettes_path {":/Palettes"},
  m_tr0 {0.0},
  m_n {0},
  m_bHaveTransmitted {false},
  m_user_defined {tr ("User Defined")}
{
  ui->setupUi(this);

  setWindowTitle (QApplication::applicationName () + " - " + tr ("Waterfall"));
  setWindowFlags (Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint);
  setMaximumHeight (MAX_VERT_BINS);

  ui->vertPlot->setCursor(Qt::CrossCursor);
  ui->cbControls->setCursor(Qt::ArrowCursor);
  ui->cbBars->setCursor(Qt::ArrowCursor);

  connect(ui->vertPlot, SIGNAL(freezeDecode1(int)),this,
          SLOT(wideFreezeDecode(int)));

  connect(ui->vertPlot, SIGNAL(setFreq1(int,int)),this,
          SLOT(setFreq2(int,int)));

  {
    //Restore user's settings
    SettingsGroup g {m_settings, "VertWaterfall"};
    restoreGeometry (m_settings->value ("geometry", saveGeometry ()).toByteArray ());
    ui->vertPlot->setPlotZero(m_settings->value("PlotZero", 0).toInt());
    ui->vertPlot->setPlotGain(m_settings->value("PlotGain", 0).toInt());
    ui->zeroSlider->setValue(ui->vertPlot->plotZero());
    ui->gainSlider->setValue(ui->vertPlot->plotGain());
    m_timestamp = m_settings->value("Timestamp",0).toInt();
    ui->timestampComboBox->setCurrentIndex(m_timestamp); ui->vertPlot->setTimestamp(m_timestamp);
    m_bars=m_settings->value("Bars",true).toBool();
    ui->cbBars->setChecked(m_bars);
    ui->vertPlot->setBars(m_bars);
    m_freq=m_settings->value("Freq",true).toBool();
    ui->cbFreq->setChecked(m_freq);
    ui->vertPlot->showFreq(m_freq);
    m_bFlatten=m_settings->value("Flatten",true).toBool();
    ui->cbFlatten->setChecked(m_bFlatten);
    ui->vertPlot->setFlatten(m_bFlatten);
    m_waterfallAvg = m_settings->value("WaterfallAvg",2).toInt();
    ui->waterfallAvgSpinBox->setValue(m_waterfallAvg);
    ui->vertPlot->setWaterfallAvg(m_waterfallAvg);
    int nbpp = m_settings->value("BinsPerPixel",8).toInt();   // 8: fit more spectrum in the height
    ui->bppSpinBox->setValue(nbpp);
    ui->vertPlot->setBinsPerPixel(nbpp);
    ui->vertPlot->setStartFreq(m_settings->value("StartFreq",0).toInt());
    ui->fStartSpinBox->setValue(ui->vertPlot->startFreq());
    m_waterfallPalette=m_settings->value("WaterfallPalette","Default").toString();
    m_userPalette = WFPalette {m_settings->value("UserPalette").value<WFPalette::Colours> ()};
    ui->controls_widget->setVisible(!m_settings->value("HideControls",false).toBool());
    ui->cbControls->setChecked(!m_settings->value("HideControls",false).toBool());
  }

  int index=0;
  for (QString const& file:
         m_palettes_path.entryList(QDir::NoDotAndDotDot |
                                   QDir::System | QDir::Hidden |
                                   QDir::AllDirs | QDir::Files,
                                   QDir::DirsFirst)) {
    QString t=file.mid(0,file.length()-4);
    ui->paletteComboBox->addItem(t);
    if(t==m_waterfallPalette) ui->paletteComboBox->setCurrentIndex(index);
    index++;
  }
  ui->paletteComboBox->addItem (m_user_defined);
  if (m_user_defined == m_waterfallPalette) ui->paletteComboBox->setCurrentIndex(index);
  readPalette ();
}

VerticalWaterfall::~VerticalWaterfall ()
{
}

void VerticalWaterfall::closeEvent (QCloseEvent * e)
{
  // Closing this window via its title bar just closes it; it is not reopened
  // automatically and does not affect the Wide Graph. Reopening is done from
  // the View menu.
  saveSettings ();
  QDialog::closeEvent (e);
  Q_EMIT closedByUser ();
}

void VerticalWaterfall::saveSettings()
{
  SettingsGroup g {m_settings, "VertWaterfall"};
  m_settings->setValue ("geometry", saveGeometry ());
  m_settings->setValue ("PlotZero", ui->vertPlot->plotZero());
  m_settings->setValue ("PlotGain", ui->vertPlot->plotGain());
  m_settings->setValue ("BinsPerPixel", ui->vertPlot->binsPerPixel ());
  m_settings->setValue ("WaterfallAvg", ui->waterfallAvgSpinBox->value ());
  m_settings->setValue ("StartFreq", ui->vertPlot->startFreq ());
  m_settings->setValue ("WaterfallPalette", m_waterfallPalette);
  m_settings->setValue ("UserPalette", QVariant::fromValue (m_userPalette.colours ()));
  m_settings->setValue ("Flatten",m_bFlatten);
  m_settings->setValue ("HideControls", ui->controls_widget->isHidden ());
  m_settings->setValue ("Timestamp",m_timestamp);
  m_settings->setValue ("Bars", m_bars);
  m_settings->setValue ("Freq", m_freq);
}

void VerticalWaterfall::dataSink2(float s[], float df3, int ihsym, int ndiskdata)  //dataSink2
{
  int nbpp = ui->vertPlot->binsPerPixel();

//Average spectra over specified number, m_waterfallAvg
  if (m_n==0) {
    for (int i=0; i<NSMAX; i++)
      m_splot[i]=s[i];
  } else {
    for (int i=0; i<NSMAX; i++)
      m_splot[i] += s[i];
  }
  m_n++;

  if (m_n>=m_waterfallAvg) {
    for (int i=0; i<NSMAX; i++)
        m_splot[i] /= m_n;        //Normalize the average
    m_n=0;
    int i=int(ui->vertPlot->startFreq()/df3 + 0.5);
    int jz=5000.0/(nbpp*df3);
    if(jz>MAX_VERT_BINS) jz=MAX_VERT_BINS;
    for (int j=0; j<jz; j++) {
      float ss=0.0;
      for (int k=0; k<nbpp; k++) {
        ss += m_splot[i++];
      }
      m_swide[j]=nbpp*ss;
    }

// Time according to this computer
    qint64 ms = DriftingDateTime::currentMSecsSinceEpoch() % 86400000;
    double tr = fmod(0.001*ms,m_TRperiod);
    if((ndiskdata && ihsym <= m_waterfallAvg) || (!ndiskdata && (tr<m_tr0))) {
      float flagValue=1.0e30;
      if(m_bHaveTransmitted) flagValue=2.0e30;
      for(int i=0; i<MAX_VERT_BINS; i++) {
        m_swide[i] = flagValue;
      }
      for(int i=0; i<NSMAX; i++) {
        m_splot[i] = flagValue;
      }
      m_bHaveTransmitted=false;
    }
    m_tr0=tr;
    ui->vertPlot->draw(m_swide,true);
  }
}

void VerticalWaterfall::on_bppSpinBox_valueChanged(int n)                   //bpp
{
  ui->vertPlot->setBinsPerPixel(n);
}

void VerticalWaterfall::on_waterfallAvgSpinBox_valueChanged(int n)          //Navg
{
  m_waterfallAvg = n;
  ui->vertPlot->setWaterfallAvg(n);
}

void VerticalWaterfall::keyPressEvent(QKeyEvent *e)                         //F11, F12
{
  switch(e->key())
  {
  int n;
  case Qt::Key_F11:
    n=11;
    if(e->modifiers() & Qt::ControlModifier) n+=100;
    emit f11f12(n);
    break;
  case Qt::Key_F12:
    n=12;
    if(e->modifiers() & Qt::ControlModifier) n+=100;
    emit f11f12(n);
    break;
  default:
    QDialog::keyPressEvent (e);
  }
}

void VerticalWaterfall::setRxFreq(int n)                                    //setRxFreq
{
  ui->vertPlot->setRxFreq(n);
}

void VerticalWaterfall::wideFreezeDecode(int n)                             //wideFreezeDecode
{
  emit freezeDecode2(n);
}

void VerticalWaterfall::setPeriod(double trperiod, int nsps)                //setPeriod
{
  m_TRperiod=trperiod;
  m_nsps=nsps;
  ui->vertPlot->setNsps(trperiod, nsps);
}

void VerticalWaterfall::setTxFreq(int n)                                    //setTxFreq
{
  ui->vertPlot->setTxFreq(n);
}

void VerticalWaterfall::setMode(QString mode)                               //setMode
{
  m_mode=mode;
  ui->vertPlot->setMode(mode);
  ui->vertPlot->DrawOverlay();
  ui->vertPlot->update();
}

void VerticalWaterfall::setSubMode(int n)                                   //setSubMode
{
  ui->vertPlot->setSubMode(n);
  ui->vertPlot->DrawOverlay();
  ui->vertPlot->update();
}

void VerticalWaterfall::setFreq2(int rxFreq, int txFreq)                    //setFreq2
{
  emit setFreq3(rxFreq,txFreq);
}

void VerticalWaterfall::setDialFreq(double d)                               //setDialFreq
{
  ui->vertPlot->setDialFreq(d);
}

void VerticalWaterfall::setRxBand (QString const& band)
{
  m_rxBand = band;
  ui->vertPlot->setRxBand(band);
}

void VerticalWaterfall::on_fStartSpinBox_valueChanged(int n)                //fStart
{
  ui->vertPlot->setStartFreq(n);
}

void VerticalWaterfall::readPalette ()                                      //readPalette
{
  try
    {
      if (m_user_defined == m_waterfallPalette)
        {
          ui->vertPlot->setColours (WFPalette {m_userPalette}.interpolate ());
        }
      else
        {
          ui->vertPlot->setColours (WFPalette {m_palettes_path.absoluteFilePath (m_waterfallPalette + ".pal")}.interpolate());
        }
    }
  catch (std::exception const& e)
    {
      MessageBox::warning_message (this, tr ("Read Palette"), e.what ());
    }
}

void VerticalWaterfall::on_paletteComboBox_activated (QString const& palette)    //palette selector
{
  m_waterfallPalette = palette;
  readPalette();
  replot();
}

void VerticalWaterfall::on_cbFlatten_toggled(bool b)                        //Flatten On/Off
{
  m_bFlatten=b;
  ui->vertPlot->setFlatten(m_bFlatten);
}

void VerticalWaterfall::on_cbControls_toggled(bool b)
{
  ui->controls_widget->setVisible(b);
}

void VerticalWaterfall::on_timestampComboBox_currentIndexChanged(int n)
{
  m_timestamp = n;
  ui->vertPlot->setTimestamp(n);
}

void VerticalWaterfall::on_cbBars_toggled(bool b)
{
  m_bars = b;
  ui->vertPlot->setBars(m_bars);
}

void VerticalWaterfall::on_cbFreq_toggled(bool b)
{
  m_freq = b;
  ui->vertPlot->showFreq(m_freq);
}

void VerticalWaterfall::on_adjust_palette_push_button_clicked (bool)   //Adjust Palette
{
  try
    {
      if (m_userPalette.design ())
        {
          m_waterfallPalette = m_user_defined;
          ui->paletteComboBox->setCurrentText (m_waterfallPalette);
          readPalette ();
        }
    }
  catch (std::exception const& e)
    {
      MessageBox::warning_message (this, tr ("Read Palette"), e.what ());
    }
}

void VerticalWaterfall::replot()
{
  if(ui->vertPlot->scaleOK()) ui->vertPlot->replot();
}

void VerticalWaterfall::on_gainSlider_valueChanged(int value)               //Gain
{
  ui->vertPlot->setPlotGain(value);
  replot();
}

void VerticalWaterfall::on_zeroSlider_valueChanged(int value)               //Zero
{
  ui->vertPlot->setPlotZero(value);
  replot();
}

void VerticalWaterfall::setSuperFox(bool b)
{
  ui->vertPlot->setSuperFox(b);
}

void VerticalWaterfall::setSuperHound(bool b)
{
  ui->vertPlot->setSuperHound(b);
}

void VerticalWaterfall::setTol(int n)                                       //setTol
{
  ui->vertPlot->setTol(n);
  ui->vertPlot->DrawOverlay();
  ui->vertPlot->update();
}

void VerticalWaterfall::setFST4_FreqRange(int fLow,int fHigh)
{
  ui->vertPlot->setFST4_FreqRange(fLow,fHigh);
}

void VerticalWaterfall::setSingleDecode(bool b)
{
  ui->vertPlot->setSingleDecode(b);
}

void VerticalWaterfall::setWSPRtransmitted()
{
  m_bHaveTransmitted=true;
}

void VerticalWaterfall::setVHF(bool bVHF)
{
  ui->vertPlot->setVHF(bVHF);
}

void VerticalWaterfall::setDiskUTC(int nutc)
{
  ui->vertPlot->setDiskUTC(nutc);
}

void VerticalWaterfall::setDarkStyle(bool b)
{
  ui->vertPlot->setDarkStyle(b);
}
