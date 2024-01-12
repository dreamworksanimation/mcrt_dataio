// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

//
//
#include "MiscUtil.h"

#include <ctime>                // std::localtime()
#include <iomanip>
#include <sstream>

#include <limits.h>             // HOST_NAME_MAX
#include <sys/time.h>           // gettimeofday()
#include <unistd.h>             // gethostname()

namespace mcrt_dataio {

// static function    
uint64_t
MiscUtil::getCurrentMicroSec()
{
    struct timeval tv;
    gettimeofday(&tv, 0x0);
    uint64_t cTime = (uint64_t)tv.tv_sec * 1000 * 1000 + (uint64_t)tv.tv_usec;
    return cTime; // return microsec from Epoch
}

// static function
float
MiscUtil::us2s(const uint64_t microsec)
{
    return (float)(microsec) / 1000.0f / 1000.0f; // microsec to sec
}

// static function
std::string
MiscUtil::timeFromEpochStr(const uint64_t microsecFromEpoch)
{
    struct timeval tv;
    tv.tv_sec = (long)(microsecFromEpoch / 1000 / 1000);
    tv.tv_usec = (long)(microsecFromEpoch - (uint64_t)(tv.tv_sec * 1000 * 1000));
    return timeFromEpochStr(tv);
}

// static function
std::string
MiscUtil::timeFromEpochStr(const struct timeval &tv)
{
    struct tm *time_st = localtime(&tv.tv_sec);

    static std::string mon[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    static std::string wday[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

    std::ostringstream ostr;
    ostr << time_st->tm_year + 1900 << "/"
         << mon[time_st->tm_mon] << "/"
         << std::setw(2) << std::setfill('0') << time_st->tm_mday << " "
         << wday[time_st->tm_wday] << " "
         << std::setw(2) << std::setfill('0') << time_st->tm_hour << ":"
         << std::setw(2) << std::setfill('0') << time_st->tm_min << ":"
         << std::setw(2) << std::setfill('0') << time_st->tm_sec << ":"
         << std::setw(3) << std::setfill('0') << tv.tv_usec / 1000;
    return ostr.str();
}

// static function
std::string
MiscUtil::currentTimeStr()
{
    uint64_t microsecFromEpoch = getCurrentMicroSec();

    struct timeval tv;
    tv.tv_sec = (long)(microsecFromEpoch / 1000 / 1000);
    tv.tv_usec = (long)(microsecFromEpoch - (uint64_t)(tv.tv_sec * 1000 * 1000));

    struct tm *time_st = localtime(&tv.tv_sec);

    static std::string mon[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    static std::string wday[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

    std::ostringstream ostr;
    ostr << time_st->tm_year + 1900
         << mon[time_st->tm_mon]
         << std::setw(2) << std::setfill('0') << time_st->tm_mday
         << wday[time_st->tm_wday] << "_"
         << std::setw(2) << std::setfill('0') << time_st->tm_hour
         << std::setw(2) << std::setfill('0') << time_st->tm_min << "_"
         << std::setw(2) << std::setfill('0') << time_st->tm_sec << "_"
         << std::setw(3) << std::setfill('0') << (tv.tv_usec / 1000);
    return ostr.str();
}

// static function
std::string
MiscUtil::secStr(const float sec)
{
    std::ostringstream ostr;
    if (sec < 60.0f) {
        ostr << sec << " sec";
    } else if (sec < 60.0f * 60.0f) {
        int m = (int)(sec / 60.0f);
        float s = sec - static_cast<float>(m) * 60.0f;
        ostr << m << " min " << s << " sec";
    } else {
        int h = (int)(sec / 60.0f / 60.0f);
        int m = (int)((sec - static_cast<float>(h) * 60.0f * 60.0f) / 60.0f);
        float s = sec - static_cast<float>(h) * 60.0f * 60.0f - static_cast<float>(m) * 60.0f;
        ostr << h << " hour " << m << " min " << s << " sec";
    }
    return ostr.str();
}

// static function
std::string
MiscUtil::getHostName()
{
    char buff[HOST_NAME_MAX];
    if(gethostname(buff, HOST_NAME_MAX) == -1) {
        return std::string("");
    }
    return std::string(buff); // return current hostName
}

} // namespace mcrt_dataio

