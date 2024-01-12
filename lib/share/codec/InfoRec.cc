// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "InfoRec.h"

#include <mcrt_dataio/share/util/MiscUtil.h>

#include <scene_rdl2/render/util/StrUtil.h>
#include <scene_rdl2/scene/rdl2/ValueContainerDeq.h>
#include <scene_rdl2/scene/rdl2/ValueContainerEnq.h>

#include <json/writer.h>

#include <fstream>
#include <limits>

// Debug message about searching rendering start/complete/finish timestamp by heuristic logic
//#define DEBUG_MSG_RENDER_TIME

namespace {

std::string
mIdStrGen(const int machineId)
{
    return std::to_string(machineId);
}

void
unitAndTitleGen(const std::string& key, std::string& title, std::string& unit)
{
    title = "?";
    unit = "?";

    if (key == "cpu" || key == "mem" || key == "prg") {
        unit = "%";
        if (key == "cpu") title = "CPU-usage";
        else if (key == "mem") title = "Memory-usage";
        else if (key == "prg") title = "Progress";
    } else if (key == "fIt") {
        unit = "sec";
        title = "FeedbackInterval";
    } else if (key == "snp" || key == "ltc" || key == "clk" || key == "fEv" || key == "fLt") {
        unit = "millisec";
        if (key == "snp") title = "Snapshot-to-Send";
        else if (key == "ltc") title = "Latency";
        else if (key == "clk") title = "ClockShift";
        else if (key == "fEv") title = "Feedback-eval";
        else if (key == "fLt") title = "Feedback-latency";
    } else if (key == "snd" || key == "rcv" || key == "fBp") {
        unit = "Mbyte/Sec";
        if (key == "snd") title = "Send-bandwitdh";
        else if (key == "rcv") title = "Receive-bandwidth";
        else if (key == "fBp") title = "Feedback-bandwidth";
    } else if (key == "rnd" || key == "fAc") {
        unit = "bool";
        if (key == "rnd") title = "RenderActive";
        else if (key == "fAc") title = "Feedback-active";
    } else if (key == "rps") {
        unit = "enum";
        title = "RenderPrepStats";
    } else if (key == "fFp") {
        unit == "fps";
        title = "Feedback-fps";
    }
}

} // namespace

namespace mcrt_dataio {

bool
InfoRecGlobal::isDispatchSet() const
{
    if (!mArray["dp"]) return false;
    return true;
}

void
InfoRecGlobal::setDispatch(const std::string& hostName,
                           const int cpuTotal,
                           const size_t memTotal)
{
    mArray["dp"]["name"] = hostName;
    mArray["dp"]["cpu"] = cpuTotal;
    mArray["dp"]["mem"] = static_cast<Json::Value::UInt64>(memTotal);
}

bool
InfoRecGlobal::isMcrtSet(const int machineId) const
{
    std::string mIdStr = mIdStrGen(machineId);
    if (!mArray["mc"][mIdStr]) return false;
    return true;
}

void
InfoRecGlobal::setMcrt(const int machineId,
                       const std::string& hostName,
                       const int cpuTotal,
                       const size_t memTotal)
{
    std::string mIdStr = mIdStrGen(machineId);
    mArray["mc"][mIdStr]["name"] = hostName;
    mArray["mc"][mIdStr]["cpu"] = cpuTotal;
    mArray["mc"][mIdStr]["mem"] = static_cast<Json::Value::UInt64>(memTotal);
}

size_t
InfoRecGlobal::getMcrtTotal() const
{
    return mArray["mc"].size();
}

bool
InfoRecGlobal::isMergeSet() const
{
    if (!mArray["mg"]) return false;
    return true;
}

void
InfoRecGlobal::setMerge(const std::string& hostName,
                        const int cpuTotal,
                        const size_t memTotal)
{
    mArray["mg"]["name"] = hostName;
    mArray["mg"]["cpu"] = cpuTotal;
    mArray["mg"]["mem"] = static_cast<Json::Value::UInt64>(memTotal);
}

std::string
InfoRecGlobal::encode() const
{
    Json::FastWriter fw;
    return fw.write(mArray);
}

bool
InfoRecGlobal::decode(const std::string &data)
{
    Json::Reader jr;
    return jr.parse(data, mArray);
}

std::string
InfoRecGlobal::show() const
{
    Json::StyledWriter jw;
    return jw.write(mArray);
}

//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------

std::string
InfoRecItem::getTimeStampStr() const
{
    if (!mTimeStamp) return std::string("");
    return MiscUtil::timeFromEpochStr(getTimeStamp());
}

void
InfoRecItem::setClient(const float latency, // sec
                       const float clockShift) // millisec
{
    mArray["cl"]["ltc"] = latency;
    mArray["cl"]["clk"] = clockShift;
}

void
InfoRecItem::setMerge(const float cpuUsage, // fraction
                      const float memUsage, // fraction
                      const float recvBps,  // Byte/Sec
                      const float sendBps,  // Byte/Sec
                      const float progress) // fraction
{
    mArray["mg"]["cpu"] = cpuUsage;
    mArray["mg"]["mem"] = memUsage;
    mArray["mg"]["rcv"] = recvBps;
    mArray["mg"]["snd"] = sendBps;
    mArray["mg"]["prg"] = progress;
}

void
InfoRecItem::setMergeFeedbackOn(const float feedbackInterval, // sec
                                const float evalFeedbackTime, // millisec
                                const float sendFeedbackFps,  // fps
                                const float sendFeedbackBps)  // Byte/Sec
{
    mArray["mg"]["fAc"] = true;
    mArray["mg"]["fIt"] = feedbackInterval; // sec
    mArray["mg"]["fEv"] = evalFeedbackTime; // millisec
    mArray["mg"]["fFp"] = sendFeedbackFps;  // fps
    mArray["mg"]["fBp"] = sendFeedbackBps;  // Byte/Sec
}

void
InfoRecItem::setMergeFeedbackOff()
{
    mArray["mg"]["fAc"] = false;
}

bool
InfoRecItem::isMergeFeedbackActive() const
{
    if (!mArray["mg"]["fAc"]) return false;
    return mArray["mg"]["fAc"].asBool();
}

float
InfoRecItem::getMergeProgress() const
{
    if (!mArray["mg"]["prg"]) return 0.0f;
    return mArray["mg"]["prg"].asFloat();
}

void
InfoRecItem::setMcrt(const int machineId,
                     const float cpuUsage,       // fraction
                     const float memUsage,       // fraction
                     const float snapshotToSend, // millisec
                     const float sendBps,        // Byte/Sec
                     const bool renderActive,    // do rendering or not
                     const int renderPrepStats,  // enum int
                     const float progress,       // fraction
                     const float clockShift)     // millisec
{
    std::string mIdStr = mIdStrGen(machineId);

    mArray["mc"][mIdStr]["mId"] = machineId;
    mArray["mc"][mIdStr]["cpu"] = cpuUsage;
    mArray["mc"][mIdStr]["mem"] = memUsage;
    mArray["mc"][mIdStr]["snp"] = snapshotToSend;
    mArray["mc"][mIdStr]["snd"] = sendBps;
    mArray["mc"][mIdStr]["rnd"] = renderActive;
    mArray["mc"][mIdStr]["rps"] = renderPrepStats;
    mArray["mc"][mIdStr]["prg"] = progress;
    mArray["mc"][mIdStr]["clk"] = clockShift;
}

void
InfoRecItem::setMcrtFeedbackOn(const int machineId,
                               const float feedbackInterval, // sec
                               const float recvFeedbackFps,  // fps
                               const float recvFeedbackBps,  // Byte/Sec
                               const float evalFeedbackTime, // millisec
                               const float feedbackLatency)  // millisec
{
    std::string mIdStr = mIdStrGen(machineId);

    mArray["mc"][mIdStr]["fAc"] = true;
    mArray["mc"][mIdStr]["fIt"] = feedbackInterval;
    mArray["mc"][mIdStr]["fFp"] = recvFeedbackFps;
    mArray["mc"][mIdStr]["fBp"] = recvFeedbackBps;
    mArray["mc"][mIdStr]["fEv"] = evalFeedbackTime;
    mArray["mc"][mIdStr]["fLt"] = feedbackLatency;
}

void
InfoRecItem::setMcrtFeedbackOff(const int machineId)
{
    std::string mIdStr = mIdStrGen(machineId);
    mArray["mc"][mIdStr]["fAc"] = false;
}

bool
InfoRecItem::isMcrtFeedbackActive(const int machineId) const
{
    std::string mIdStr = mIdStrGen(machineId);
    if (!mArray["mc"][mIdStr]["fAc"]) return false;
    return mArray["mc"][mIdStr]["fAc"].asBool();
}

float
InfoRecItem::getMcrtSummedProgress() const // return total fraction
{
    Json::Value jv = mArray["mc"];
    if (jv.empty()) {
        return 0.0f;
    }

    float progressTotal = 0.0f;
    for (Json::ValueIterator itr = jv.begin(); itr != jv.end(); ++itr) {
        float currProgress = (*itr)["prg"].asFloat();
        if (currProgress > 0.0f) progressTotal += currProgress;
    }
    return progressTotal;
}

bool
InfoRecItem::isMcrtAllStop() const
{
    Json::Value jv = mArray["mc"];
    if (jv.empty()) {
        return true; // no entry => all stop
    }

    for (Json::ValueIterator itr = jv.begin(); itr != jv.end(); ++itr) {
        if ((*itr)["rnd"].asBool()) {
            return false;       // found active mcrt => return false;
        }
    }
    return true;                // can not find active mcrt => all stop
}

bool
InfoRecItem::isMcrtAllStart() const
{
    Json::Value jv = mArray["mc"];
    if (jv.empty()) {
        return false; // no entry => all stop
    }

    for (Json::ValueIterator itr = jv.begin(); itr != jv.end(); ++itr) {
        if (!(*itr)["rnd"].asBool()) {
            return false;       // found non active mcrt => return false;
        }
    }
    return true;                // can not find non active mcrt => all start
}

std::string
InfoRecItem::encode() const
{
    Json::FastWriter fw;
    return fw.write(mArray);
}

bool
InfoRecItem::decode(const std::string &data)
{
    Json::Reader jr;
    bool flag = jr.parse(data, mArray);

    mTimeStamp = mArray["time"].asUInt64();

    return flag;
}

std::string
InfoRecItem::show() const
{
    Json::StyledWriter jw;
    return jw.write(mArray);
}

std::string
InfoRecItem::showTable(const std::string &key) const
{
    //
    // Unit and Title
    //
    std::string title, unit;
    unitAndTitleGen(key, title, unit);

    //------------------------------
    //
    // setup data array
    //
    std::deque<bool> bVec;
    std::vector<float> fVec;
    std::vector<int> iVec;
    size_t totalMcrt = 0;

    // get mcrt data as array
    if (key == "rnd" || // RenderActive status
        key == "fAc") { // FeedbackActive condition
        bVec = getMcrtValAsBool(key);
        totalMcrt = bVec.size();
    } else if (key == "rps") { // RenderPrepStatus
        iVec = getMcrtValAsInt(key);
        totalMcrt = iVec.size();
    } else { // others
        fVec = getMcrtValAsFloat(key);
        totalMcrt = fVec.size();
    }
    
    //------------------------------
    //
    // output
    //
    std::ostringstream ostr;
    if (key == "rcv" || key == "ltc") {
        ostr << title << ' ' << unit;
    } else {
        ostr << title << ' ' << unit << " (total-mcrt:" << totalMcrt << ')';
    }
    if (key == "cpu" || key == "mem" || key == "snd" || key == "prg" || key == "rcv" ||
        key == "fAc" || key == "fBp" || key == "fFp" || key == "fEv" || key == "fIt") {
        if (key == "fAc") {
            ostr << " mg:" << showVal(getMergeValAsBool(key));
        } else {
            ostr << " mg:" << std::setw(4) << std::fixed << std::setprecision(1) << getMergeValAsFloat(key);
        }
    }
    if (key == "ltc" || key == "clk") {
        ostr << " cl:" << std::setw(4) << std::fixed << std::setprecision(1) << getClientValAsFloat(key);
    }

    ostr << " " << getTimeStampStr();
    if (key != "rcv" && key != "ltc") {
        ostr << " {\n";
        if (key == "rnd" || key == "fAc") {
            ostr << scene_rdl2::str_util::addIndent(showArray(bVec, 10)) << '\n';
        } else if (key == "rps") {
            ostr << scene_rdl2::str_util::addIndent(showArray(iVec, 10)) << '\n';
        } else {
            ostr << scene_rdl2::str_util::addIndent(showArray(fVec, 10)) << '\n';
        }
        ostr << "}";
    }
    return ostr.str();
}

std::deque<bool>
InfoRecItem::getMcrtValAsBool(const std::string &key) const
{
    std::deque<bool> vec(getMaxMachineId() + 1, 0.0f);
    crawlAllMcrt([&](Json::Value &jvMcrt) {
            vec[jvMcrt["mId"].asInt()] = jvMcrt[key].asBool();
        });
    return vec;
}

std::vector<int>
InfoRecItem::getMcrtValAsInt(const std::string &key) const
{
    std::vector<int> vec(getMaxMachineId() + 1, 0.0f);
    crawlAllMcrt([&](Json::Value &jvMcrt) {
            vec[jvMcrt["mId"].asInt()] = jvMcrt[key].asInt();
        });
    return vec;
}

std::vector<float>
InfoRecItem::getMcrtValAsFloat(const std::string &key) const
{
    std::vector<float> vec(getMaxMachineId() + 1, 0.0f);
    crawlAllMcrt([&](Json::Value &jvMcrt) {
            vec[jvMcrt["mId"].asInt()] = getSingleMcrtValAsFloat(jvMcrt, key);
        });
    return vec;
}

float
InfoRecItem::getOpMcrtValAsFloat(const std::string &key, OpType opType) const
{
    int mcrtTotal = 0;
    float sum = 0.0f;
    float avg = 0.0f;
    float min = std::numeric_limits<float>::max();
    float max = std::numeric_limits<float>::min();
    crawlAllMcrt([&](Json::Value &jvMcrt) {
            float v = getSingleMcrtValAsFloat(jvMcrt, key);
            switch (opType) {
            case OpType::SUM : sum += v; break;
            case OpType::AVG : avg += v; mcrtTotal++; break;
            case OpType::MIN : if (v < min) min = v; break;
            case OpType::MAX : if (max < v) max = v; break;
            default : break;
            }
        });

    float result = 0.0f;
    switch (opType) {
    case OpType::SUM : result = sum; break;
    case OpType::AVG : result = (mcrtTotal)? (avg / static_cast<float>(mcrtTotal)): 0.0f; break;
    case OpType::MIN : result = min; break;
    case OpType::MAX : result = max; break;
    default : break;
    }
    return result;
}

// static function
InfoRecItem::OpType
InfoRecItem::opTypeFromKey(const std::string &opKey)
{
    if (opKey == "sum") {
        return OpType::SUM;
    } else if (opKey == "avg") {
        return OpType::AVG;
    } else if (opKey == "min") {
        return OpType::MIN;
    } else if (opKey == "max") {
        return OpType::MAX;
    }
    return OpType::NOP;
}

bool
InfoRecItem::getMergeValAsBool(const std::string& key) const
{
    return mArray["mg"][key].asBool();
}

float
InfoRecItem::getMergeValAsFloat(const std::string &key) const
{
    float fVal = 0.0f;
    if (key == "cpu" || key == "mem" || key == "prg") {
        fVal = mArray["mg"][key].asFloat() * 100.0f; // convert to percentage
    } else if (key == "rcv" || key == "snd" ||
               key == "fBp") {
        fVal = mArray["mg"][key].asFloat() / 1024.0f / 1024.0f; // convert to MByte/Sec
    } else if (key == "fFp" || key == "fEv" || key == "fIt") {
        fVal = mArray["mg"][key].asFloat();
    }
    return fVal;
}

float    
InfoRecItem::getClientValAsFloat(const std::string &key) const
{
    float fVal = 0.0f;
    if (key == "ltc") {
        fVal = mArray["cl"][key].asFloat() * 1000.0f; // sec to millisec conversion
    } else if(key == "clk") {
        fVal = mArray["cl"][key].asFloat();
    }
    return fVal;
}

std::deque<bool>
InfoRecItem::getAllValAsBool(const std::string &key, const size_t totalMcrt) const
{
    std::deque<bool> mcrtVec = getMcrtValAsBool(key);
    std::deque<bool> vec(totalMcrt + 2, 0); // 2 extra for merge and client
    size_t max = (mcrtVec.size() > totalMcrt)? totalMcrt: mcrtVec.size();
    for (size_t i = 0; i < max; ++i) {
        vec[i] = mcrtVec[i];
    }
    vec[totalMcrt] = getMergeValAsBool(key);
    // we don't have bool value for client at this moment.
    return vec;
}

std::vector<int>
InfoRecItem::getAllValAsInt(const std::string &key, const size_t totalMcrt) const
{
    std::vector<int> mcrtVec = getMcrtValAsInt(key);
    std::vector<int> vec(totalMcrt + 2, 0); // 2 extra for merge and client
    size_t max = (mcrtVec.size() > totalMcrt)? totalMcrt: mcrtVec.size();
    for (size_t i = 0; i < max; ++i) {
        vec[i] = mcrtVec[i];
    }
    // We don't have int value for merge and client at this moment.
    return vec;
}

std::vector<float>
InfoRecItem::getAllValAsFloat(const std::string &key, const size_t totalMcrt) const
{
    std::vector<float> mcrtVec = getMcrtValAsFloat(key);
    std::vector<float> vec(totalMcrt + 2, 0.0f); // 2 extra for merge and client
    size_t max = (mcrtVec.size() > totalMcrt)? totalMcrt: mcrtVec.size();
    for (size_t i = 0; i < max; ++i) {
        vec[i] = mcrtVec[i];
    }
    vec[totalMcrt] = getMergeValAsFloat(key);
    vec[totalMcrt + 1] = getClientValAsFloat(key);
    return vec;
}

//------------------------------------------------------------------------------------------

void
InfoRecItem::setTimeStamp()
{
    mTimeStamp = MiscUtil::getCurrentMicroSec();
    mArray["time"] = static_cast<Json::Value::UInt64>(mTimeStamp);
}

int
InfoRecItem::getMaxMachineId() const
{
    int max = 0;
    crawlAllMcrt([&](Json::Value &jvMcrt) {
            int mId = jvMcrt["mId"].asInt();
            if (max < mId) max = mId;
        });
    return max;
}

float
InfoRecItem::getSingleMcrtValAsFloat(const Json::Value &jvMcrt, const std::string &key) const
{
    float fVal = 0.0f;
    if (key == "cpu" || key == "mem" || key == "prg") {
        fVal = jvMcrt[key].asFloat() * 100.0f;
    } else if (key == "snd" || key == "fBp") {
        fVal = jvMcrt[key].asFloat() / 1024.0f / 1024.0f;
    } else if (key == "snp" || key == "clk" || key == "fFp" || key == "fEv" || key == "fIt" || key == "fLt") {
        fVal = jvMcrt[key].asFloat();
    }
    return fVal;
}

std::string
InfoRecItem::showArray(const std::deque<bool> &vec, int oneLineMaxItem) const
{
    std::ostringstream ostr;
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) { ostr << ((!(i % oneLineMaxItem))? '\n': ' '); }
        ostr << ((vec[i]) ? "T" : "F");
    }
    return ostr.str();
}

std::string
InfoRecItem::showArray(const std::vector<int> &vec, int oneLineMaxItem) const
{
    auto vecMax = [&](const std::vector<int> &vec) -> int {
        int max = 0;
        for (size_t i = 0; i < vec.size(); ++i) {
            if (i == 0) {
                max = vec[i];
            } else {
                if (max < vec[i]) max = vec[i];
            }
        }
        return max;
    };
    int w = static_cast<int>(std::to_string(vecMax(vec)).size());

    std::ostringstream ostr;
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) { ostr << ((!(i % oneLineMaxItem))? '\n': ' '); }
        ostr << std::setw(w) << vec[i];
    }
    return ostr.str();
}

std::string
InfoRecItem::showArray(const std::vector<float> &vec, int oneLineMaxItem) const
{
    std::ostringstream ostr;
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) { ostr << ((!(i % oneLineMaxItem))? '\n': ' '); }
        ostr << std::setw(4) << std::fixed << std::setprecision(1) << vec[i];
    }
    return ostr.str();
}

std::string
InfoRecItem::showVal(const float v) const
{
    std::ostringstream ostr;
    ostr << std::setw(4) << std::fixed << std::setprecision(1) << v;
    return ostr.str();
}

std::string
InfoRecItem::showVal(const bool v) const
{
    return (v) ? "T" : "F";
}

void
InfoRecItem::crawlAllMcrt(std::function<void(Json::Value &jvMcrt)> func) const
{
    Json::Value jv = mArray["mc"];
    if (jv.empty()) return;     // empty mcrt information

    for (Json::ValueIterator itr = jv.begin(); itr != jv.end(); ++itr) {
        func((*itr));
    }
}

//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------

void
InfoRecMaster::clearItems()
{
    mData.clear();
    mLastTimeStamp = 0;
}

InfoRecMaster::InfoRecItemShPtr
InfoRecMaster::newRecItem()
{
    mData.emplace_back(std::make_shared<InfoRecItem>());
    InfoRecItemShPtr currItem = mData.back();
    mLastTimeStamp = currItem->getTimeStamp();
    return currItem;
}

InfoRecMaster::InfoRecItemShPtr
InfoRecMaster::getRecItem(const size_t id) const
{
    if (id >= mData.size()) return InfoRecItemShPtr(nullptr);

    int i = 0;
    for (auto itr = mData.begin(); itr != mData.end(); ++itr) {
        if (static_cast<size_t>(i) == id) return (*itr);
        i++;
    }
    return InfoRecItemShPtr(nullptr);
}

bool
InfoRecMaster::intervalCheck(const float intervalSec) const
{
    uint64_t delta = MiscUtil::getCurrentMicroSec() - mLastTimeStamp;
    float deltaSec = (float)delta / 1000.0f / 1000.0f;
    return (deltaSec > intervalSec);
}

void
InfoRecMaster::encode(scene_rdl2::rdl2::ValueContainerEnq &vcEnq) const
{
    std::string data = mGlobal.encode();
    vcEnq.enqString(data);

    vcEnq.enq<size_t>(mData.size());
    for (auto itr = mData.begin(); itr != mData.end(); ++itr) {
        vcEnq.enqString((*itr)->encode());
    }
}

bool
InfoRecMaster::decode(scene_rdl2::rdl2::ValueContainerDeq &vcDeq)
{
    std::string data = vcDeq.deqString();
    mGlobal.decode(data);

    size_t total = vcDeq.deq<size_t>();
    for (size_t i = 0; i < total; ++i) {
        InfoRecItemShPtr recItem = newRecItem();
        if (!recItem->decode(vcDeq.deqString())) {
            return false;
        }
    }
    return true;
}

bool
InfoRecMaster::save(const std::string &filename, const std::string &extension) const
{
    std::string data;
    scene_rdl2::rdl2::ValueContainerEnq vcEnq(&data);
    encode(vcEnq);
    size_t dataSize = vcEnq.finalize();

    std::ostringstream ostr;
    ostr << filename << MiscUtil::currentTimeStr() << extension;
    std::string currFilename = ostr.str();

    std::ofstream out(currFilename, std::ios::trunc | std::ios::binary);
    if (!out) {
        std::cerr << "Could not open file '" << currFilename << "' for writing infoRec" << std::endl;
        return false;
    }

    out.write((char *)&dataSize, sizeof(size_t));
    out.write(&data[0], dataSize);

    return true;
}

bool
InfoRecMaster::load(const std::string &filename)
{
    std::ifstream in(filename.c_str(), std::ios::binary);
    if (!in) {
        std::cerr << "Could not open file '" << filename << "' for reading infoRec" << std::endl;
        return false;
    }

    size_t dataSize;
    in.read((char *)&dataSize, sizeof(size_t));

    std::string data(dataSize, '\0');
    in.read(&(data[0]), dataSize);

    scene_rdl2::rdl2::ValueContainerDeq vcDeq(data.data(), dataSize);
    if (!decode(vcDeq)) {
        std::cerr << "Dequeue infoRec failed. filename:" << filename << std::endl;
        return false;
    }
    return true;
}

std::string
InfoRecMaster::show() const
{
    std::ostringstream ostr;
    ostr << "InfoRecMaster {\n";
    ostr << scene_rdl2::str_util::addIndent(mGlobal.show()) << '\n'
         << "  mData (size:" << mData.size() << ") {\n";
    for (auto itr = mData.begin(); itr != mData.end(); ++itr) {
        ostr << scene_rdl2::str_util::addIndent((*itr)->show(), 2) << '\n';
    }
    ostr << "  }\n"
         << "}";
    return ostr.str();
}

std::string    
InfoRecMaster::showTable(const std::string &key) const
{
    //
    // Unit and Title
    //
    std::string title, unit;
    unitAndTitleGen(key, title, unit);

    //------------------------------
    //
    // setup data array
    //
    std::vector<std::deque<bool>> bVec2D;
    std::vector<std::vector<float>> fVec2D;
    std::vector<float> fVec1D;
    std::vector<std::vector<int>> iVec2D;
    size_t totalData = 0;

    if (key == "rnd" || // RenderActive status
        key == "fAc") { // FeedbackActive condition
        bVec2D = getAllValAsBool(key);
        totalData = bVec2D.size();
    } else if (key == "rcv") { // Receive (merge)
        fVec1D = getMergeValAsFloat(key);
        totalData = fVec1D.size();
    } else if (key == "ltc") { // Latency (client)
        fVec1D = getClientValAsFloat(key);
        totalData = fVec1D.size();
    } else if (key == "rps") { // RenderPrepStatus
        iVec2D = getAllValAsInt(key);
        totalData = iVec2D.size();
    } else { // others
        fVec2D = getAllValAsFloat(key);
        totalData = fVec2D.size();
    }

    std::vector<uint64_t> timeStamp = getTimeStamp();

    //------------------------------
    //
    // output
    //
    std::ostringstream ostr;
    if (key == "rcv" || key == "ltc") {
        ostr << title << ' ' << unit;
    } else {
        ostr << title << ' ' << unit << " (total-data:" << totalData << ')'; 
    }

    ostr << " {\n";
    if (key == "rnd" || key == "fAc") {
        ostr << scene_rdl2::str_util::addIndent(showArray2DHead(timeStamp, bVec2D)) << '\n';
        ostr << scene_rdl2::str_util::addIndent(showArray2D(timeStamp, bVec2D)) << '\n';
    } else if (key == "rps") {
        ostr << scene_rdl2::str_util::addIndent(showArray2DHead(timeStamp, iVec2D)) << '\n';
        ostr << scene_rdl2::str_util::addIndent(showArray2D(timeStamp, iVec2D)) << '\n';
    } else if (key == "rcv" || key == "ltc") {
        ostr << scene_rdl2::str_util::addIndent(showArray1D(timeStamp, fVec1D)) << '\n';
    } else {
        ostr << scene_rdl2::str_util::addIndent(showArray2DHead(timeStamp, fVec2D)) << '\n';
        ostr << scene_rdl2::str_util::addIndent(showArray2D(timeStamp, fVec2D)) << '\n';
    }
    ostr << "}";
    return ostr.str();
}

std::string
InfoRecMaster::showRenderSpan() const
{
    uint64_t startTimeStamp, completeTimeStamp, finishTimeStamp;
    calcRenderSpan(startTimeStamp, completeTimeStamp, finishTimeStamp);

    std::ostringstream ostr;
    ostr << "renderTime {\n"
         << "  mcrtTotal:" << mGlobal.getMcrtTotal() << '\n'
         << "      start:" << MiscUtil::timeFromEpochStr(startTimeStamp) << '\n';
    if (completeTimeStamp) {
        float duration = MiscUtil::us2s(completeTimeStamp - startTimeStamp);
        ostr << "   complete:" << MiscUtil::timeFromEpochStr(completeTimeStamp)
             << " duration:" << duration << " sec" << " (" << MiscUtil::secStr(duration) << ")\n";
    } else {
        ostr << "   complete: ?\n";
    }
    if (finishTimeStamp) {
        float overrun = MiscUtil::us2s(finishTimeStamp - completeTimeStamp);
        ostr << "     finish:" << MiscUtil::timeFromEpochStr(finishTimeStamp)
             << "  overrun:" << overrun << " sec" << " (" << MiscUtil::secStr(overrun) << ")\n";
    } else {
        ostr << "     finish: ?\n";
    }
    ostr << "}";

    return ostr.str();
}

std::string
InfoRecMaster::showRenderSpanOpValMcrt(const std::string &key,
                                       const std::string &opKeyA, // for timeStamp
                                       const std::string &opKeyB, // for MCRT
                                       int timeStampSkipOffset) const
{
    InfoRecItem::OpType opTypeA = InfoRecItem::opTypeFromKey(opKeyA);
    InfoRecItem::OpType opTypeB = InfoRecItem::opTypeFromKey(opKeyB);
    if (opTypeA == InfoRecItem::OpType::NOP || opTypeB == InfoRecItem::OpType::NOP) {
        std::cerr << "invalid opKeyA:" << opKeyA << " and/or opKeyB:" << opKeyB << std::endl;
        return "";
    }

    uint64_t startTimeStamp, completeTimeStamp, finishTimeStamp;
    float result = renderSpanOpMain(opTypeA,
                                    timeStampSkipOffset,
                                    [&](const InfoRecItemShPtr infoRecItemShPtr) -> float {
                                        return infoRecItemShPtr->getOpMcrtValAsFloat(key, opTypeB);
                                    },
                                    startTimeStamp,
                                    completeTimeStamp,
                                    finishTimeStamp);

    //------------------------------

    float duration = MiscUtil::us2s(completeTimeStamp - startTimeStamp);

    std::ostringstream ostr;
    ostr << "MCRT average value {\n"
         << "   mcrtTotal:" << mGlobal.getMcrtTotal() << '\n'
         << "       start:" << MiscUtil::timeFromEpochStr(startTimeStamp) << '\n'
         << "    complete:" << MiscUtil::timeFromEpochStr(completeTimeStamp)
         << " duration:" << duration << " sec" << " (" << MiscUtil::secStr(duration) << ")\n"
         << "  skipOffset:" << timeStampSkipOffset << '\n'
         << "         key:" << key << '\n'
         << "      opKeyA:" << opKeyA << " for timeStamp\n"
         << "      opKeyB:" << opKeyB << " for MCRT\n"
         << "} result:" << result;
    return ostr.str();
}

std::string
InfoRecMaster::showRenderSpanOpValMerge(const std::string &key,
                                        const std::string &opKey) const
{
    InfoRecItem::OpType opType = InfoRecItem::opTypeFromKey(opKey);
    if (opType == InfoRecItem::OpType::NOP) {
        std::cerr << "invalid opKey:" << opKey << std::endl;
        return "";
    }

    uint64_t startTimeStamp, completeTimeStamp, finishTimeStamp;
    float result = renderSpanOpMain(opType,
                                    0, // timeStampSkipOffset
                                    [&](const InfoRecItemShPtr infoRecItemShPtr) -> float {
                                        return infoRecItemShPtr->getMergeValAsFloat(key);
                                    },
                                    startTimeStamp,
                                    completeTimeStamp,
                                    finishTimeStamp);

    //------------------------------

    float duration = MiscUtil::us2s(completeTimeStamp - startTimeStamp);

    std::ostringstream ostr;
    ostr << "Merge average value {\n"
         << "   mcrtTotal:" << mGlobal.getMcrtTotal() << '\n'
         << "       start:" << MiscUtil::timeFromEpochStr(startTimeStamp) << '\n'
         << "    complete:" << MiscUtil::timeFromEpochStr(completeTimeStamp)
         << " duration:" << duration << " sec" << " (" << MiscUtil::secStr(duration) << ")\n"
         << "         key:" << key << '\n'
         << "       opKey:" << opKey << '\n'
         << "} result:" << result;
    return ostr.str();
}

std::string
InfoRecMaster::showRenderSpanOpValClient(const std::string &key,
                                         const std::string &opKey) const
{
    InfoRecItem::OpType opType = InfoRecItem::opTypeFromKey(opKey);
    if (opType == InfoRecItem::OpType::NOP) {
        std::cerr << "invalid opKey:" << opKey << std::endl;
        return "";
    }

    uint64_t startTimeStamp, completeTimeStamp, finishTimeStamp;
    float result = renderSpanOpMain(opType,
                                    0, // timeStampSkipOffset
                                    [&](const InfoRecItemShPtr infoRecItemShPtr) -> float {
                                        return infoRecItemShPtr->getClientValAsFloat(key);
                                    },
                                    startTimeStamp,
                                    completeTimeStamp,
                                    finishTimeStamp);

    //------------------------------

    float duration = MiscUtil::us2s(completeTimeStamp - startTimeStamp);

    std::ostringstream ostr;
    ostr << "Client average value {\n"
         << "   mcrtTotal:" << mGlobal.getMcrtTotal() << '\n'
         << "       start:" << MiscUtil::timeFromEpochStr(startTimeStamp) << '\n'
         << "    complete:" << MiscUtil::timeFromEpochStr(completeTimeStamp)
         << " duration:" << duration << " sec" << " (" << MiscUtil::secStr(duration) << ")\n"
         << "         key:" << key << '\n'
         << "       opKey:" << opKey << '\n'
         << "} result:" << result;
    return ostr.str();
}

std::string
InfoRecMaster::showRenderSpanAllValMcrt(const std::string &key) const
{
    uint64_t startTimeStamp, completeTimeStamp, finishTimeStamp;
    calcRenderSpan(startTimeStamp, completeTimeStamp, finishTimeStamp);
    if (startTimeStamp == 0 || completeTimeStamp == 0) {
        std::cerr << "could not find render complete timeStamp => early exit" << std::endl;
        return "";
    }

    int totalMcrt = static_cast<int>(mGlobal.getMcrtTotal());
    int totalItems = calcItemTotal(startTimeStamp, completeTimeStamp);
    std::vector<std::vector<float>> vec(totalItems, std::vector<float>(0));

    int id = 0;
    crawlAllRenderItems(startTimeStamp, completeTimeStamp,
                        [&](const InfoRecItemShPtr infoRecItemShPtr) {
                            vec[id++] = infoRecItemShPtr->getAllValAsFloat(key, totalMcrt);
                        });

    //------------------------------

    std::ostringstream ostr;
    ostr << "# key:" << key << '\n'
         << "# start:" << MiscUtil::timeFromEpochStr(startTimeStamp) << '\n'
         << "# complete:" << MiscUtil::timeFromEpochStr(completeTimeStamp) << '\n'
         << "# totalTimeStamp:" << totalItems << '\n'
         << "# totalMcrt:" << totalMcrt << '\n'
         << "# timeStampId key-val[mcrtId=0] key-val[mcrtId=1] ...\n";

    // for matrix type 2d data array for splot command of gnuplot
    for (int timeId = 0; timeId < totalItems; ++timeId) {
        for (int mcrtId = 0; mcrtId < totalMcrt; ++mcrtId) {
            if (mcrtId > 0) ostr << ' ';
            ostr << vec[timeId][mcrtId];
        }
        ostr << "\n";
    }

    /* regular 3d data array : useful printout code for general purpose
    for (int mcrtId = 0; mcrtId < totalMcrt; ++mcrtId) {
        ostr << "# mcrtId:" << mcrtId << '\n';
        for (int timeId = 0; timeId < totalItems; ++timeId) {
            ostr << mcrtId << ' ' << timeId << ' ' << vec[timeId][mcrtId] << '\n';
        }
        ostr << "\n\n";
    }
    */

    return ostr.str();
}

std::string
InfoRecMaster::showRenderSpanValMerge(const std::string &key) const
{
    uint64_t startTimeStamp, completeTimeStamp, finishTimeStamp;
    calcRenderSpan(startTimeStamp, completeTimeStamp, finishTimeStamp);
    if (startTimeStamp == 0 || completeTimeStamp == 0) {
        std::cerr << "could not find render complete timeStamp => early exit" << std::endl;
        return "";
    }

    int totalItems = calcItemTotal(startTimeStamp, completeTimeStamp);
    std::vector<float> vec(totalItems);
    int id = 0;
    crawlAllRenderItems(startTimeStamp, completeTimeStamp,
                        [&](const InfoRecItemShPtr infoRecItemShPtr) {
                            vec[id++] = infoRecItemShPtr->getMergeValAsFloat(key);
                        });

    //------------------------------

    std::ostringstream ostr;
    ostr << "# key:" << key << '\n'
         << "# start:" << MiscUtil::timeFromEpochStr(startTimeStamp) << '\n'
         << "# complete:" << MiscUtil::timeFromEpochStr(completeTimeStamp) << '\n'
         << "# totalTimeStamp:" << totalItems << '\n'
         << "# timeStampId key-val\n";

    // for matrix type 2d data array for splot command of gnuplot
    for (int timeId = 0; timeId < totalItems; ++timeId) {
        if (timeId > 0) ostr << '\n';
        ostr << timeId << ' ' << vec[timeId];
    }

    return ostr.str();
}

std::string
InfoRecMaster::showRenderSpanValClient(const std::string &key) const
{
    uint64_t startTimeStamp, completeTimeStamp, finishTimeStamp;
    calcRenderSpan(startTimeStamp, completeTimeStamp, finishTimeStamp);
    if (startTimeStamp == 0 || completeTimeStamp == 0) {
        std::cerr << "could not find render complete timeStamp => early exit" << std::endl;
        return "";
    }

    int totalItems = calcItemTotal(startTimeStamp, completeTimeStamp);
    std::vector<float> vec(totalItems);
    int id = 0;
    crawlAllRenderItems(startTimeStamp, completeTimeStamp,
                        [&](const InfoRecItemShPtr infoRecItemShPtr) {
                            vec[id++] = infoRecItemShPtr->getClientValAsFloat(key);
                        });

    //------------------------------

    std::ostringstream ostr;
    ostr << "# key:" << key << '\n'
         << "# start:" << MiscUtil::timeFromEpochStr(startTimeStamp) << '\n'
         << "# complete:" << MiscUtil::timeFromEpochStr(completeTimeStamp) << '\n'
         << "# totalTimeStamp:" << totalItems << '\n'
         << "# timeStampId key-val\n";

    // for matrix type 2d data array for splot command of gnuplot
    for (int timeId = 0; timeId < totalItems; ++timeId) {
        if (timeId > 0) ostr << '\n';
        ostr << timeId << ' ' << vec[timeId];
    }

    return ostr.str();
}



std::vector<uint64_t>
InfoRecMaster::getTimeStamp() const
{
    std::vector<uint64_t> vec(getItemTotal(), 0);
    int id = 0;
    for (auto itr = mData.begin(); itr != mData.end(); ++itr) {
        vec[id++] = (*itr)->getTimeStamp();
    }
    return vec;
}

std::vector<float>
InfoRecMaster::getMergeValAsFloat(const std::string &key) const
{
    std::vector<float> vec(getItemTotal(), 0.0f);
    int id = 0;
    for (auto itr = mData.begin(); itr != mData.end(); ++itr) {
        vec[id++] = (*itr)->getMergeValAsFloat(key);
    }
    return vec;
}

std::vector<float>
InfoRecMaster::getClientValAsFloat(const std::string &key) const
{
    std::vector<float> vec(getItemTotal(), 0.0f);
    int id = 0;
    for (auto itr = mData.begin(); itr != mData.end(); ++itr) {
        vec[id++] = (*itr)->getClientValAsFloat(key);
    }
    return vec;
}

std::vector<std::deque<bool>>
InfoRecMaster::getAllValAsBool(const std::string &key) const
{
    int totalMcrt = static_cast<int>(mGlobal.getMcrtTotal());
    std::vector<std::deque<bool>> vec(getItemTotal(), std::deque<bool>(false));
    int id = 0;
    for (auto itr = mData.begin(); itr != mData.end(); ++itr) {
        vec[id++] = (*itr)->getAllValAsBool(key, totalMcrt);
    }
    return vec;
}

std::vector<std::vector<int>>
InfoRecMaster::getAllValAsInt(const std::string &key) const
{
    int totalMcrt = static_cast<int>(mGlobal.getMcrtTotal());
    std::vector<std::vector<int>> vec(getItemTotal(), std::vector<int>(0));
    int id = 0;
    for (auto itr = mData.begin(); itr != mData.end(); ++itr) {
        vec[id++] = (*itr)->getAllValAsInt(key, totalMcrt);
    }
    return vec;
}

std::vector<std::vector<float>>
InfoRecMaster::getAllValAsFloat(const std::string &key) const
{
    int totalMcrt = static_cast<int>(mGlobal.getMcrtTotal());
    std::vector<std::vector<float>> vec(getItemTotal(), std::vector<float>(0));
    int id = 0;
    for (auto itr = mData.begin(); itr != mData.end(); ++itr) {
        vec[id++] = (*itr)->getAllValAsFloat(key, totalMcrt);
    }
    return vec;
}

std::string
InfoRecMaster::showMcrt(const std::string& key, const unsigned startId, const unsigned endId) const
{
    std::ostringstream ostr;
    ostr << "# showMcrt key:" << key << " startDataId:" << startId << " endDataId:" << endId << '\n'
         << "# id deltaSec mcrt ...\n";

    int w = scene_rdl2::str_util::getNumberOfDigits(endId - startId + 1);
    uint64_t startTimeStamp = 0; // microsec from epoch
    unsigned id = 0;
    unsigned id2 = 0;
    for (auto itr = mData.begin(); itr != mData.end(); ++itr) {
        if (startId <= id && id <= endId) {
            if (id2 == 0) startTimeStamp = (*itr)->getTimeStamp(); // 1st data
            float sec = MiscUtil::us2s((*itr)->getTimeStamp() - startTimeStamp);

            std::vector<float> vec = (*itr)->getMcrtValAsFloat(key);

            ostr << std::setw(w) << id2 << ' ' << sec << ' ';
            for (size_t j = 0; j < vec.size(); ++j) {
                ostr << vec[j] << ' ';
            }
            ostr << '\n';
            id2++;
        }
        id++;
    }
    return ostr.str();
}

std::string
InfoRecMaster::showMcrtAvg(const std::string& key, const unsigned startId, const unsigned endId) const
{
    std::ostringstream ostr;
    ostr << "# showMcrtAvg key:" << key << " startDataId:" << startId << " endDataId:" << endId << '\n'
         << "# id deltaSec val\n";

    int w = scene_rdl2::str_util::getNumberOfDigits(endId - startId + 1);
    uint64_t startTimeStamp = 0; // microsec from epoch
    unsigned id = 0;
    unsigned id2 = 0;
    for (auto itr = mData.begin(); itr != mData.end(); ++itr) {
        if (startId <= id && id <= endId) {
            if (id2 == 0) startTimeStamp = (*itr)->getTimeStamp(); // 1st data
            float sec = MiscUtil::us2s((*itr)->getTimeStamp() - startTimeStamp);

            float v = (*itr)->getOpMcrtValAsFloat(key, InfoRecItem::OpType::AVG);
            ostr << std::setw(w) << id2 << ' ' << sec << ' ' << v << '\n';
            id2++;
        }
        id++;
    }
    return ostr.str();
}

std::string
InfoRecMaster::showMerge(const std::string& key, const unsigned startId, const unsigned endId) const
{
    std::ostringstream ostr;
    ostr << "# showMerge key:" << key << " startDataId:" << startId << " endDataId:" << endId << '\n'
         << "# id deltaSec val\n";

    int w = scene_rdl2::str_util::getNumberOfDigits(endId - startId + 1);
    uint64_t startTimeStamp = 0; // microsec from epoch
    unsigned id = 0;
    unsigned id2 = 0;
    for (auto itr = mData.begin(); itr != mData.end(); ++itr) {
        if (startId <= id && id <= endId) {
            if (id2 == 0) startTimeStamp = (*itr)->getTimeStamp(); // 1st data
            float sec = MiscUtil::us2s((*itr)->getTimeStamp() - startTimeStamp);

            float v = (*itr)->getMergeValAsFloat(key);
            ostr << std::setw(w) << id2 << ' ' << sec << ' ' << v << '\n';
            id2++;
        }
        id++;
    }
    return ostr.str();
}

//------------------------------------------------------------------------------------------

void
InfoRecMaster::calcRenderSpan(uint64_t &startTimeStamp,        // return 0 if undefined
                              uint64_t &completeTimeStamp,     // return 0 if undefined
                              uint64_t &finishTimeStamp) const // return 0 if undefined
{
    startTimeStamp = 0;
    completeTimeStamp = 0;
    finishTimeStamp = 0;

    float prevProgress = 0.0f;
    bool mcrtAllStart = false;
    for (auto itr = mData.begin(); itr != mData.end(); ++itr) {
        float currProgress = (*itr)->getMcrtSummedProgress();
        uint64_t currTimeStamp = (*itr)->getTimeStamp();
        bool isMcrtAllStart = (*itr)->isMcrtAllStart();
        bool isMcrtAllStop = (*itr)->isMcrtAllStop();

#       ifdef DEBUG_MSG_RENDER_TIME
        if (currProgress < 0.01f || 0.99f < currProgress) {
            std::cerr << ">> InfoRec.cc "
                      << MiscUtil::timeFromEpochStr(currTimeStamp)
                      << " currProgress:"
                      << std::setw(5) << std::fixed << std::setprecision(3) << currProgress
                      << " allStart:" << ((isMcrtAllStart)? "T" : "F")
                      << " allStop:" << ((isMcrtAllStop)? "T" : "F")
                      << std::endl;
        }
#       endif // end DEBUG_MSG_RENDER_TIME

        if (currProgress < prevProgress) {
            prevProgress = 0.0f;
            startTimeStamp = 0;
            completeTimeStamp = 0;
            finishTimeStamp = 0;
#           ifdef DEBUG_MSG_RENDER_TIME
            std::cerr << "reset" << std::endl;
#           endif // end DEBUG_MSG_RENDER_TIME
        }

        if (!startTimeStamp) {
            if (prevProgress < currProgress && currProgress > 0.0f) {
                startTimeStamp = currTimeStamp;
#               ifdef DEBUG_MSG_RENDER_TIME
                std::cerr << "start:" << MiscUtil::timeFromEpochStr(currTimeStamp) << std::endl;
#               endif // end DEBUG_MSG_RENDER_TIME
            }
        } else if (!completeTimeStamp) {
            if (prevProgress < 1.0f && 1.0f <= currProgress) {
                completeTimeStamp = currTimeStamp;
#               ifdef DEBUG_MSG_RENDER_TIME
                std::cerr << "complete:" << MiscUtil::timeFromEpochStr(currTimeStamp) << std::endl;
#               endif // end DEBUG_MSG_RENDER_TIME
            }
        }

        if (!mcrtAllStart) {
            mcrtAllStart = isMcrtAllStart;
        } else {
            if (isMcrtAllStop) {
                finishTimeStamp = currTimeStamp;
#               ifdef DEBUG_MSG_RENDER_TIME
                std::cerr << "finish:" << MiscUtil::timeFromEpochStr(currTimeStamp) << std::endl;
#               endif // end DEBUG_MSG_RENDER_TIME
                break;
            }
        }
        prevProgress = currProgress;
    }
}

int
InfoRecMaster::calcItemTotal(const uint64_t startTimeStamp,
                             const uint64_t endTimeStamp) const
{
    int total = 0;
    crawlAllRenderItems(startTimeStamp, endTimeStamp, [&](const InfoRecItemShPtr) { total++; });
    return total;
}

void
InfoRecMaster::
crawlAllRenderItems(const uint64_t startTimeStamp,
                    const uint64_t completeTimeStamp,
                    std::function<void(const InfoRecItemShPtr infoRecItemShPtr)> func) const
{
    if (startTimeStamp == 0 || completeTimeStamp == 0) return;

    for (auto itr = mData.begin(); itr != mData.end(); ++itr) {
        uint64_t currTimeStamp = (*itr)->getTimeStamp();
        if (startTimeStamp <= currTimeStamp && currTimeStamp <= completeTimeStamp) {
            func((*itr));
        }
    }
}

float
InfoRecMaster::renderSpanOpMain(const InfoRecItem::OpType opType,
                                int timeStampSkipOffset,
                                std::function<float(const InfoRecItemShPtr infoRecItemShPtr)> func,
                                uint64_t &startTimeStamp,
                                uint64_t &completeTimeStamp,                                
                                uint64_t &finishTimeStamp) const
{
    //    uint64_t startTimeStamp, completeTimeStamp, finishTimeStamp;
    calcRenderSpan(startTimeStamp, completeTimeStamp, finishTimeStamp);
    if (startTimeStamp == 0 || completeTimeStamp == 0) {
        return 0.0f;
    }

    float sum = 0.0f;
    float avg = 0.0f;
    float min = std::numeric_limits<float>::max();
    float max = std::numeric_limits<float>::min();
    int itemTotal = 0;
    int skipTotal = 0;
    crawlAllRenderItems(startTimeStamp, completeTimeStamp,
                        [&](const InfoRecItemShPtr infoRecItemShPtr) {
                            if (skipTotal < timeStampSkipOffset) {
                                skipTotal++;
                            } else {
                                float v = func(infoRecItemShPtr);
                                switch (opType) {
                                case InfoRecItem::OpType::SUM : sum += v; break;
                                case InfoRecItem::OpType::AVG : avg += v; itemTotal++; break;
                                case InfoRecItem::OpType::MIN : if (v < min) min = v; break;
                                case InfoRecItem::OpType::MAX : if (max < v) max = v; break;
                                default : break;
                                }
                            }
                        });

    float result = 0.0f;
    switch (opType) {
    case InfoRecItem::OpType::SUM : result = sum; break;
    case InfoRecItem::OpType::AVG :
        result = (itemTotal)? (avg / static_cast<float>(itemTotal)): 0.0f;
        break;
    case InfoRecItem::OpType::MIN : result = min; break;
    case InfoRecItem::OpType::MAX : result = max; break;
    default : break;
    }

    return result;
}

std::string
InfoRecMaster::showArray2DHead(const std::vector<uint64_t> &timeStamp,
                               const std::vector<std::deque<bool>> &vec) const
{
    std::ostringstream ostr;
    
    int w = static_cast<int>(std::to_string(vec.size()).size());
    if (w == 1) ostr << "i";
    else        ostr << std::setw(w) << "id";
    ostr << ' ';

    int wT = static_cast<int>(MiscUtil::timeFromEpochStr(timeStamp[0]).size()) + 2;
    ostr << std::setw(wT) << "timestamp" << "  ";

    size_t mergeId = vec[0].size() - 2;
    size_t clientId = vec[0].size() - 1;
    for (size_t j = 0; j < vec[0].size(); ++j) {
        if (j < mergeId)        ostr << "m ";
        else if (j == mergeId)  ostr << "g ";
        else if (j == clientId) ostr << "c";
    }
    ostr << "  m:mcrt g:merge c:client";
    return ostr.str();
}

std::string
InfoRecMaster::showArray2D(const std::vector<uint64_t> &timeStamp,
                           const std::vector<std::deque<bool>> &vec) const
{
    std::ostringstream ostr;
    int w = static_cast<int>(std::to_string(vec.size()).size());
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i != 0) ostr << '\n';
        ostr << std::setw(w) << i << ' ';
        for (size_t j = 0; j < vec[i].size(); ++j) {
            if (j == 0) ostr << "[" << MiscUtil::timeFromEpochStr(timeStamp[i]) << "] ";
            ostr << ' ' << ((vec[i][j]) ? "T" : "F");
        }
    }
    return ostr.str();
}

std::string
InfoRecMaster::showArray2DHead(const std::vector<uint64_t> &timeStamp,
                               const std::vector<std::vector<float>> &vec) const
{
    std::ostringstream ostr;
    
    int w = static_cast<int>(std::to_string(vec.size()).size());
    if (w == 1) ostr << "i";
    else        ostr << std::setw(w) << "id";
    ostr << ' ';

    int wT = static_cast<int>(MiscUtil::timeFromEpochStr(timeStamp[0]).size()) + 2;
    ostr << std::setw(wT) << "timestamp" << "  ";

    size_t mergeId = vec[0].size() - 2;
    size_t clientId = vec[0].size() - 1;
    for (size_t j = 0; j < vec[0].size(); ++j) {
        if (j < mergeId)        ostr << "mcrt ";
        else if (j == mergeId)  ostr << "merg ";
        else if (j == clientId) ostr << "clnt";
    }
    return ostr.str();
}

std::string
InfoRecMaster::showArray2D(const std::vector<uint64_t> &timeStamp,
                           const std::vector<std::vector<float>> &vec) const
{
    std::ostringstream ostr;
    int w = static_cast<int>(std::to_string(vec.size()).size());
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i != 0) ostr << '\n';
        ostr << std::setw(w) << i << ' ';
        for (size_t j = 0; j < vec[i].size(); ++j) {
            if (j == 0) ostr << "[" << MiscUtil::timeFromEpochStr(timeStamp[i]) << "] ";
            ostr << ' ' << std::setw(4) << std::fixed << std::setprecision(1) << vec[i][j];
        }
    }
    return ostr.str();
}

std::string
InfoRecMaster::showArray2DHead(const std::vector<uint64_t> &timeStamp,
                               const std::vector<std::vector<int>> &vec) const
{
    int wI = 0;
    for (size_t i = 0; i < vec.size(); ++i) {
        for (size_t j = 0; j < vec[i].size(); ++j) {
            int currW = static_cast<int>(std::to_string(vec[i][j]).size());
            if (wI < currW) wI = currW;
        }
    }

    std::ostringstream ostr;
    int w = static_cast<int>(std::to_string(vec.size()).size());
    if (w == 1) ostr << "i";
    else        ostr << std::setw(w) << "id";
    ostr << ' ';

    int wT = static_cast<int>(MiscUtil::timeFromEpochStr(timeStamp[0]).size()) + 2;
    ostr << std::setw(wT) << "timestamp" << "  ";

    size_t mergeId = vec[0].size() - 2;
    size_t clientId = vec[0].size() - 1;
    if (wI < 4) {
        for (size_t j = 0; j < vec[0].size(); ++j) {
            ostr << std::setw(wI);
            if (j < mergeId)        ostr << "m";
            else if (j == mergeId)  ostr << "g";
            else if (j == clientId) ostr << "c";
            ostr << ' ';
        }
        ostr << "  m:mcrt g:merge c:client";
    } else {
        for (size_t j = 0; j < vec[0].size(); ++j) {
            ostr << std::setw(wI);
            if (j < mergeId)        ostr << "mcrt";
            else if (j == mergeId)  ostr << "merg";
            else if (j == clientId) ostr << "clnt";
            ostr << ' ';
        }
    }
    return ostr.str();
}

std::string
InfoRecMaster::showArray2D(const std::vector<uint64_t> &timeStamp,
                           const std::vector<std::vector<int>> &vec) const
{
    int wI = 0;
    for (size_t i = 0; i < vec.size(); ++i) {
        for (size_t j = 0; j < vec[i].size(); ++j) {
            int currW = static_cast<int>(std::to_string(vec[i][j]).size());
            if (wI < currW) wI = currW;
        }
    }

    std::ostringstream ostr;
    int w = static_cast<int>(std::to_string(vec.size()).size());
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i != 0) ostr << '\n';
        ostr << std::setw(w) << i << ' ';
        for (size_t j = 0; j < vec[i].size(); ++j) {
            if (j == 0) ostr << "[" << MiscUtil::timeFromEpochStr(timeStamp[i]) << "] ";
            ostr << ' ' << std::setw(wI) << vec[i][j];
        }
    }
    return ostr.str();
}

std::string
InfoRecMaster::showArray1D(const std::vector<uint64_t> &timeStamp,
                           const std::vector<float> &vec) const
{
    std::ostringstream ostr;
    int w = static_cast<int>(std::to_string(vec.size()).size());
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i != 0) ostr << '\n';
        ostr << std::setw(w) << i << ' ';
        ostr << "[" << MiscUtil::timeFromEpochStr(timeStamp[i]) << "] ";
        ostr << ' ' << std::setw(4) << std::fixed << std::setprecision(1) << vec[i];
    }
    return ostr.str();
}

} // namespace mcrt_dataio
