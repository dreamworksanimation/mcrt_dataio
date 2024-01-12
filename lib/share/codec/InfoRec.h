// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <json/json.h>

#include <deque>
#include <functional>           // function
#include <list>
#include <memory>               // shared_ptr

//
// All InfoRec related classes are used for recording arras related rendering runtime
// statistical information.
// This recording operation is implemented at client (inside of ClientReceiverFb)
// and client application should manage recording statistical information by this
// InfoRec class.
// Main purpose of recording statistical information is debug and analyzing performance.
// Final output InfoRec data is JSON based information and pretty easy to access internal
// data. This InfoRec information is very powerful and useful to analyze performance of
// arras related rendering sessions.
//

namespace scene_rdl2 {
namespace rdl2 {
    class ValueContainerDeq;
    class ValueContainerEnq;
}
}

namespace mcrt_dataio {

class InfoRecGlobal
//
// This is a global information for a single rendering session which is recorded.
// This class mainly keeps global information like total number of HTcore and each
// back-end hostname and physical information (HTcore/memory).
// All information which is updated on each timing during the session should not
// be here and they should be recorded by InfoRecItem inside InfoRecMaster.
// InfoRecGlobal should keep static environment information only.
// This class stores all information as JSON objects internally.
//
{
public:
    bool isDispatchSet() const;
    void setDispatch(const std::string& hostName,
                     const int cpuTotal,
                     const size_t memTotal);

    bool isMcrtSet(const int machineId) const;
    void setMcrt(const int machineId,
                 const std::string &hostName,
                 const int cpuTotal,
                 const size_t memTotal);
    size_t getMcrtTotal() const;

    bool isMergeSet() const;
    void setMerge(const std::string &hostName,
                  const int cpuTotal,
                  const size_t memTotal);

    std::string encode() const;
    bool decode(const std::string &data);

    std::string show() const;

private:
    Json::Value mArray;
};

class InfoRecItem
//
// This class is used to keep statistical information at some particular timing of
// the rendering session.
// This class includes all back-end mcrt engine + merge node information.
// This class stores all information as JSON objects internally.
//
{
public:
    enum class OpType : int {
        NOP,
        SUM,
        AVG,
        MIN,
        MAX
    };

    InfoRecItem()
    {
        setTimeStamp();
    }

    uint64_t getTimeStamp() const { return mTimeStamp; }
    std::string getTimeStampStr() const;

    void setClient(const float latency,      // sec
                   const float clockShift);  // millisec

    void setMerge(const float cpuUsage,      // fraction
                  const float memUsage,      // fraction
                  const float recvBps,       // Byte/Sec
                  const float sendBps,       // Byte/Sec
                  const float progress);     // fraction
    void setMergeFeedbackOn(const float feedbackInterval, // sec
                            const float evalFeedbackTime, // millisec
                            const float sendFeedbackFps,  // fps
                            const float sendFeedbackBps); // Byte/Sec
    void setMergeFeedbackOff();
    bool isMergeFeedbackActive() const;
    float getMergeProgress() const;
                  
    void setMcrt(const int machineId,
                 const float cpuUsage,       // fraction
                 const float memUsage,       // fraction
                 const float snapshotToSend, // millisec
                 const float sendBps,        // Byte/Sec
                 const bool renderActive,    // do rendering or not
                 const int renderPrepStats,  // enum int
                 const float progress,       // fraction
                 const float clockShift);    // millisec
    void setMcrtFeedbackOn(const int machineId,
                           const float feedbackInterval, // sec
                           const float recvFeedbackFps,  // fps
                           const float recvFeedbackBps,  // Byte/Sec
                           const float evalFeedbackTime, // millisec
                           const float feedbackLatency); // millisec
    void setMcrtFeedbackOff(const int machineId);
    bool isMcrtFeedbackActive(const int machineId) const;
    float getMcrtSummedProgress() const; // return mcrt's summed progress fraction regarding to the all mcrt
    
    bool isMcrtAllStop() const;
    bool isMcrtAllStart() const;

    std::string encode() const;
    bool decode(const std::string &data);

    std::string show() const;
    std::string showTable(const std::string &key) const;

    // access functions
    std::deque<bool> getMcrtValAsBool(const std::string& key) const;
    std::vector<int> getMcrtValAsInt(const std::string &key) const;
    std::vector<float> getMcrtValAsFloat(const std::string &key) const;

    float getOpMcrtValAsFloat(const std::string &key, OpType opType) const;
    static OpType opTypeFromKey(const std::string &opKey);

    bool getMergeValAsBool(const std::string& key) const;
    float getMergeValAsFloat(const std::string &key) const;
    float getClientValAsFloat(const std::string &key) const;

    // mcrt + merge + client
    //   vec[0 ~ totalMcrt-1] : mcrt value
    //   vec[totalMcrt]       : merge value
    //   vec[totalMcrt+1]     : client value
    std::deque<bool> getAllValAsBool(const std::string &key, const size_t totalMcrt) const;
    std::vector<int> getAllValAsInt(const std::string &key, const size_t totalMcrt) const;
    std::vector<float> getAllValAsFloat(const std::string &key, const size_t totalMcrt) const;

private:
    uint64_t mTimeStamp;
    Json::Value mArray;

    void setTimeStamp();
    int getMaxMachineId() const;
    float getSingleMcrtValAsFloat(const Json::Value &jvMcrt, const std::string &key) const;

    std::string showArray(const std::deque<bool> &vec, int oneLineMaxItem) const; // for bool vector
    std::string showArray(const std::vector<int> &vec, int oneLineMaxItem) const;
    std::string showArray(const std::vector<float> &vec, int oneLineMaxItem) const;
    std::string showVal(const float v) const;
    std::string showVal(const bool v) const; // bool
    void crawlAllMcrt(std::function<void(Json::Value &jvMcrt)> func) const;
};

class InfoRecMaster
//
// This class is used to keep entire statistical information for a rendering session
// which consists of globalInfo and multiple infoRecItems information.
// Save API creates files but it is not simple JSON ASCII format.
// In order to load the data, you should use the load API.
// This class also has several different access functions to get particular information
// inside statistical info. Using these API, you can easily create a small program to
// dump infoRec data as you need.
//    
{
public:
    InfoRecMaster() :
        mLastTimeStamp(0)
    {}

    using InfoRecItemShPtr = std::shared_ptr<InfoRecItem>;

    InfoRecGlobal &getGlobal() { return mGlobal; }

    void clearItems();

    size_t getItemTotal() const { return mData.size(); }
    InfoRecItemShPtr newRecItem();
    InfoRecItemShPtr getLastRecItem() { return mData.back(); }
    InfoRecItemShPtr getRecItem(const size_t id) const;

    bool intervalCheck(const float intervalSec) const;

    void encode(scene_rdl2::rdl2::ValueContainerEnq &vcEnq) const;
    bool decode(scene_rdl2::rdl2::ValueContainerDeq &vcDeq);

    bool save(const std::string &filename, const std::string &extension) const;
    bool load(const std::string &filename);

    std::string show() const;
    std::string showTable(const std::string &key) const;
    std::string showRenderSpan() const; // show renderTime info

    // calc processed key value of all timeStamp from render start to complete
    // at each timeStamp, all MCRT values are processed by opKey argument and convert to single value
    std::string showRenderSpanOpValMcrt(const std::string &key,
                                        const std::string &opKeyA, // for timeStamp
                                        const std::string &opKeyB, // for MCRT
                                        int timeStampSkipOffset) const;
    
    // calc processed key value of all timeStamp from render start to complete
    // processing type is defined by opKey
    std::string showRenderSpanOpValMerge(const std::string &key,
                                         const std::string &opKey) const;
    std::string showRenderSpanOpValClient(const std::string &key,
                                          const std::string &opKey) const;

    std::string showRenderSpanAllValMcrt(const std::string &key) const;
    std::string showRenderSpanValMerge(const std::string &key) const;
    std::string showRenderSpanValClient(const std::string &key) const;

    std::vector<uint64_t> getTimeStamp() const;
    std::vector<float> getMergeValAsFloat(const std::string &key) const;
    std::vector<float> getClientValAsFloat(const std::string &key) const;

    std::vector<std::deque<bool>> getAllValAsBool(const std::string &key) const;
    std::vector<std::vector<int>> getAllValAsInt(const std::string &key) const;
    std::vector<std::vector<float>> getAllValAsFloat(const std::string &key) const;

    std::string showMcrt(const std::string& key, const unsigned startId, const unsigned endId) const;
    std::string showMcrtAvg(const std::string& key, const unsigned startId, const unsigned endId) const;
    std::string showMerge(const std::string& key, const unsigned startId, const unsigned endId) const;

private:
    uint64_t mLastTimeStamp;

    InfoRecGlobal mGlobal;
    std::list<InfoRecItemShPtr> mData;

    void calcRenderSpan(uint64_t &startTimeStamp,         // return 0 if undefined
                        uint64_t &completeTimeStamp,      // return 0 if undefined
                        uint64_t &finishTimeStamp) const; // return 0 if undefined
    int calcItemTotal(const uint64_t startTimeStamp, const uint64_t endTimeStamp) const;

    void crawlAllRenderItems(const uint64_t startTimeStamp,
                             const uint64_t completeTimeStamp,
                             std::function<void(const InfoRecItemShPtr infoRecItemShPtr)> func) const;

    float renderSpanOpMain(const InfoRecItem::OpType opType,
                           int timeStampSkipOffset,
                           std::function<float(const InfoRecItemShPtr infoRecItemShPtr)> func,
                           uint64_t &startTimeStamp,
                           uint64_t &completeTimeStamp,                                
                           uint64_t &finishTimeStamp) const;

    std::string showArray2DHead(const std::vector<uint64_t> &timeStamp,
                                const std::vector<std::deque<bool>> &vec) const;
    std::string showArray2D(const std::vector<uint64_t> &timeStamp,
                            const std::vector<std::deque<bool>> &vec) const;

    std::string showArray2DHead(const std::vector<uint64_t> &timeStamp,
                                const std::vector<std::vector<float>> &vec) const;
    std::string showArray2D(const std::vector<uint64_t> &timeStamp,
                            const std::vector<std::vector<float>> &vec) const;

    std::string showArray2DHead(const std::vector<uint64_t> &timeStamp,
                                const std::vector<std::vector<int>> &vec) const;
    std::string showArray2D(const std::vector<uint64_t> &timeStamp,
                            const std::vector<std::vector<int>> &vec) const;

    std::string showArray1D(const std::vector<uint64_t> &timeStamp,
                            const std::vector<float> &vec) const;
};

} // namespace mcrt_dataio
