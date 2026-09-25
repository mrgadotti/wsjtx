#ifndef DRIFTING_DATE_TIME_HPP__
#define DRIFTING_DATE_TIME_HPP__

#include <QDateTime>

// Application clock: the system clock corrected by the offset measured
// against an NTP server. Use instead of QDateTime::current*() wherever
// T/R period timing or UTC stamps matter.
namespace DriftingDateTime
{
  QDateTime currentDateTime ();
  QDateTime currentDateTimeUtc ();
  qint64 currentMSecsSinceEpoch ();

  void setOffsetMSecs (qint64);
  qint64 offsetMSecs ();
}

#endif
