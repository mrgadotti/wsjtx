#include "DriftingDateTime.hpp"

#include <atomic>

namespace
{
  // read from the audio threads, written from the GUI thread
  std::atomic<qint64> offset_ms {0};
}

namespace DriftingDateTime
{
  QDateTime currentDateTime ()
  {
    return QDateTime::currentDateTime ().addMSecs (offset_ms.load ());
  }

  QDateTime currentDateTimeUtc ()
  {
    return QDateTime::currentDateTimeUtc ().addMSecs (offset_ms.load ());
  }

  qint64 currentMSecsSinceEpoch ()
  {
    return QDateTime::currentMSecsSinceEpoch () + offset_ms.load ();
  }

  void setOffsetMSecs (qint64 ms)
  {
    offset_ms.store (ms);
  }

  qint64 offsetMSecs ()
  {
    return offset_ms.load ();
  }
}
