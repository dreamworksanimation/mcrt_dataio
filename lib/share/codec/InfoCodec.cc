// Copyright 2023-2025 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0
#include "InfoCodec.h"

#include <json/json.h>
#include <json/writer.h>
#include <mutex>
#include <sstream> // istringstream

namespace mcrt_dataio {

class InfoCodec::Impl
{
public:
    using Key = const std::string;

    Impl(const Key& infoKey, const bool decodeOnly) :
        mInfoKey(infoKey),
        mDecodeOnly(decodeOnly),
        mArray(Json::arrayValue)
    {}

    Key& getInfoKey() const { return mInfoKey; }
    bool getDecodeOnly() const { return mDecodeOnly; }

    void clear(); // MTsafe
    bool isEmpty(); // MTsafe

    //------------------------------

    template <typename T>
    void set(const Key& key, const T setVal, T* setTarget)
    {
        if (!mDecodeOnly) {
            std::lock_guard<std::mutex> lock(mArrayMutex);    

            if (setTarget) *setTarget = setVal;

            Json::Value jv;
            jv[key] = setVal;
            pushBack(jv);

        } else {
            if (setTarget) {
                std::lock_guard<std::mutex> lock(mArrayMutex);
                *setTarget = setVal;
            }
        }
    }

    template <typename T>
    void setVec(const Key& key, const std::vector<T>& setVal, std::vector<T>* setTarget)
    {
        if (!mDecodeOnly) {
            std::lock_guard<std::mutex> lock(mArrayMutex);

            if (setTarget) *setTarget = setVal;
        
            Json::Value jv;
            jv[key] = convertVecToStr<T>(setVal);
            pushBack(jv);

        } else {
            if (setTarget) {
                std::lock_guard<std::mutex> lock(mArrayMutex);
                *setTarget = setVal;
            }
        }
    }

    template <typename T>
    std::string convertVecToStr(const std::vector<T>& vec) const
    {
        std::string str;
        str = std::to_string(vec.size()); // store size first
        for (size_t i = 0; i < vec.size(); ++i) {
            str += ' ';
            str += std::to_string(vec[i]);
        }
        return str;
    }

    template <typename T>
    std::vector<T> convertRealVecFromStr(const std::string& str) const
    {
        std::vector<T> vec;

        std::istringstream istr(str);
        std::string token;
        int id = 0;
        while (istr >> token) {
            if (id == 0) {
                size_t size = std::stoi(token);
                vec.resize(size);
            } else {
                vec[id - 1] = static_cast<T>(std::stod(token));
            }
            id++;
        }
        return vec;
    }

    template <typename T>
    void setVec3(const Key& key, const scene_rdl2::math::Vec3<T>& setVal, scene_rdl2::math::Vec3<T>* setTarget)
    {
        if (!mDecodeOnly) {
            std::lock_guard<std::mutex> lock(mArrayMutex);

            if (setTarget) *setTarget = setVal;
        
            Json::Value jv;
            jv[key] = convertVec3ToStr<T>(setVal);
            pushBack(jv);

        } else {
            if (setTarget) {
                std::lock_guard<std::mutex> lock(mArrayMutex);
                *setTarget = setVal;
            }
        }
    }

    template <typename T>
    std::string convertVec3ToStr(const scene_rdl2::math::Vec3<T>& v3) const
    {
        return std::to_string(v3[0]) + ' ' + std::to_string(v3[1]) + ' ' + std::to_string(v3[2]);
    }

    template <typename T>
    scene_rdl2::math::Vec3<T> convertRealVec3FromStr(const std::string& str) const
    {
        scene_rdl2::math::Vec3<T> v3;

        std::istringstream istr(str);
        std::string token;
        for (int i = 0; i < 3; ++i) {
            istr >> token;
            v3[i] = static_cast<T>(std::stod(token));
        }
        return v3;
    }

    bool encode(std::string& outputData); // MTsafe
    void encodeChild(const Key& childKey, InfoCodec::Impl& child); // MTsafe
    void encodeTable(const Key& tableKey, const Key& itemKey, InfoCodec::Impl& item); // MTsafe

    //------------------------------

    template <typename F>
    bool
    get(const Key& key, F setFunc)
    {
        Json::Value jv = mCom[key];
        if (jv.empty()) {
            return false; // key value mismatch, skip and return, this is not a error
        }
        setFunc(jv);
        return true;
    }

    // return -1 : error (parse faile or decodeFunc failed)
    //         0 ~ positive # : parsed items count
    int decode(const std::string& inputData, std::function<bool()> decodeFunc);

    // return true:decode-data false:not-decode-data(not-error)
    bool decodeChild(const Key& childKey, std::string& childInputData);

    // return true:decode-data false:not-decode-data(not-error)
    bool decodeTable(const Key& tableKey, std::string& itemKey, std::string& itemInputData);

    //------------------------------

    std::string show() const;

private:
    void pushBack(const Json::Value& jv);

    //------------------------------

    Key mInfoKey;
    bool mDecodeOnly;

    //------------------------------
    //
    // encode related parameters
    //
    std::mutex mArrayMutex;
    Json::Value mArray;

    //------------------------------
    //
    // decode related parameters
    //
    Json::Value mGroup;
    Json::Value mCom;
};

void
InfoCodec::Impl::clear() // MTsafe
{
    std::lock_guard<std::mutex> lock(mArrayMutex);
    mArray.clear();
}

bool
InfoCodec::Impl::isEmpty() // MTsafe
{
    std::lock_guard<std::mutex> lock(mArrayMutex);
    return (mArray.size() == 0);
}

void
InfoCodec::Impl::encodeChild(const Key& childKey, InfoCodec::Impl& child) // MTsafe
{
    if (mDecodeOnly) return;
    if (!child.mArray.size()) return;

    {
        std::lock_guard<std::mutex> lock(mArrayMutex);

        Json::Value jv;
        jv[childKey][child.mInfoKey] = child.mArray;

        pushBack(jv);
        child.clear();
    }
}

void
InfoCodec::Impl::encodeTable(const Key& tableKey, const Key& itemKey, InfoCodec::Impl& item) // MTsafe
{
    if (mDecodeOnly) return;
    if (!item.mArray.size()) return;

    {
        std::lock_guard<std::mutex> lock(mArrayMutex);

        Json::Value jv;
        jv[tableKey][itemKey][item.mInfoKey] = item.mArray;

        pushBack(jv);
        item.clear();
    }
}

bool    
InfoCodec::Impl::encode(std::string& outputData) // MTsafe
{
    if (!mDecodeOnly) {
        std::lock_guard<std::mutex> lock(mArrayMutex);
        if (!mArray.size()) {
            outputData.clear(); // just in case, we clean up outputData
            return false; // no encode data. This is not a error
        }

        Json::Value jv;
        jv[mInfoKey] = mArray;

        Json::FastWriter fw;
        outputData = fw.write(jv);

        mArray.clear();
    }
    return true;
}

void
InfoCodec::Impl::pushBack(const Json::Value& jv)
{
    mArray.append(jv);
}

int
InfoCodec::Impl::decode(const std::string& inputData, std::function<bool()> decodeFunc)
//
// return -1 : error (parse failed or decodeFunc failed)
//         0 ~ positive # : parsed items count
//
{
    Json::Reader jr;
    Json::Value jRoot;
    if (!jr.parse(inputData, jRoot)) {
        return -1;           // parse error
    }

    mGroup = jRoot[mInfoKey];

    int total = 0;
    for (int id = 0; id < (int)mGroup.size(); ++id) {
        mCom = mGroup[id];
        if (!decodeFunc()) {
            return -1;          // decodeFunc failed
        }
        total++;
    }

    return total;
}

bool
InfoCodec::Impl::decodeChild(const Key& childKey, std::string& childInputData)
{
    Json::Value jv = mCom[childKey];
    if (jv.empty()) {
        return false; // key value mismatch, skip and return, this is not a error
    }
    Json::FastWriter fw;
    childInputData = fw.write(jv);
    return true;
}

bool
InfoCodec::Impl::decodeTable(const Key& tableKey, std::string& itemKey, std::string& itemInputData)
{
    Json::Value jv = mCom[tableKey];
    if (jv.empty()) {
        return false; // key value mismatch, skip and return, this is not a error        
    }

    std::vector<std::string> itemKeys = jv.getMemberNames();
    if (itemKeys.size() != 1) {
        return false; // itemKeys format is wrong, skip and return
    }
    itemKey = itemKeys[0];

    Json::FastWriter fw;
    itemInputData = fw.write(jv[itemKey]);
    return true;
}

std::string
InfoCodec::Impl::show() const
{
    Json::StyledWriter jw;
    return jw.write(mArray);
}

//==========================================================================================

InfoCodec::InfoCodec(const Key& infoKey, const bool decodeOnly)
{
    mImpl.reset(new Impl(infoKey, decodeOnly));
}
 
InfoCodec::~InfoCodec()
{
}

InfoCodec::Key&
InfoCodec::getInfoKey() const
{
    return mImpl->getInfoKey();
}

bool
InfoCodec::getDecodeOnly() const
{
    return mImpl->getDecodeOnly();
}

void
InfoCodec::clear() // MTsafe
{
    mImpl->clear();
}

bool
InfoCodec::isEmpty() // MTsafe
{
    return mImpl->isEmpty();
}

void
InfoCodec::setBool(const Key& key, const bool setVal, bool* setTarget) // MTsafe
{
    mImpl->set<bool>(key, setVal, setTarget);
}

void
InfoCodec::setInt(const Key& key, const int setVal, int* setTarget) // MTsafe
{
    mImpl->set<int>(key, setVal, setTarget);
}

void
InfoCodec::setUInt(const Key& key, const unsigned int setVal, unsigned int* setTarget) // MTsafe
{
    mImpl->set<unsigned int>(key, setVal, setTarget);
}

void
InfoCodec::setInt64(const Key& key, const int64_t setVal, int64_t* setTarget) // MTsafe
{
    mImpl->set<Json::Int64>(key, (Json::Int64)setVal, (Json::Int64*)setTarget);
}

void
InfoCodec::setUInt64(const Key& key, const uint64_t setVal, uint64_t* setTarget) // MTsafe
{
    mImpl->set<Json::UInt64>(key, (Json::UInt64)setVal, (Json::UInt64*)setTarget);
}

void
InfoCodec::setSizeT(const Key& key, const size_t setVal, size_t* setTarget) // MTsafe
{
    setUInt64(key, (uint64_t)setVal, (uint64_t*)setTarget);
}

void
InfoCodec::setFloat(const Key& key, const float setVal, float* setTarget) // MTsafe
{
    mImpl->set<float>(key, setVal, setTarget);
}

void
InfoCodec::setDouble(const Key& key, const double setVal, double* setTarget) // MTsafe
{
    mImpl->set<double>(key, setVal, setTarget);
}

void 
InfoCodec::setString(const Key& key, const std::string& setVal, std::string* setTarget) // MTsafe
{
    mImpl->set<std::string>(key, setVal, setTarget);
}

void
InfoCodec::setVecFloat(const Key& key, const std::vector<float>& setVal, std::vector<float>* setTarget) // MTsafe
{
    mImpl->setVec<float>(key, setVal, setTarget);
}

void
InfoCodec::setVec3f(const Key& key, const scene_rdl2::math::Vec3f& setVal, scene_rdl2::math::Vec3f* setTarget) // MTsafe
{
    mImpl->setVec3<float>(key, setVal, setTarget);
}

bool
InfoCodec::encode(std::string& outputData) // MTsafe
{
    return mImpl->encode(outputData);
}

void
InfoCodec::encodeChild(const Key& childKey, InfoCodec& child) // MTsafe
{
    mImpl->encodeChild(childKey, *child.mImpl);
}

void
InfoCodec::encodeTable(const Key& tableKey, const Key& itemKey, InfoCodec& item) // MTsafe
{
    mImpl->encodeTable(tableKey, itemKey, *item.mImpl);
}

bool
InfoCodec::getBool(const Key& key, bool& v)
{
    return mImpl->get(key, [&](Json::Value& jv) { v = jv.asBool(); });
}

bool
InfoCodec::getInt(const Key& key, int& v)
{
    return mImpl->get(key, [&](Json::Value& jv) { v = jv.asInt(); });
}

bool
InfoCodec::getUInt(const Key& key, unsigned int& v)
{
    return mImpl->get(key, [&](Json::Value& jv) { v = jv.asUInt(); });
}

bool
InfoCodec::getInt64(const Key& key, int64_t& v)
{
    return mImpl->get(key, [&](Json::Value& jv) { v = jv.asInt64(); });
}

bool
InfoCodec::getUInt64(const Key& key, uint64_t& v)
{
    return mImpl->get(key, [&](Json::Value& jv) { v = jv.asUInt64(); });
}

bool
InfoCodec::getSizeT(const Key& key, size_t& v)
{
    return getUInt64(key, (uint64_t&)v);
}

bool
InfoCodec::getFloat(const Key& key, float& v)
{
    return mImpl->get(key, [&](Json::Value& jv) { v = jv.asFloat(); });
}

bool
InfoCodec::getDouble(const Key& key, double& v)
{
    return mImpl->get(key, [&](Json::Value& jv) { v = jv.asDouble(); });
}

bool    
InfoCodec::getString(const Key& key, std::string& v)
{
    return mImpl->get(key, [&](Json::Value& jv) { v = jv.asString(); });
}

bool
InfoCodec::getVecFloat(const Key& key, std::vector<float>& vec)
{
    return mImpl->get(key, [&](Json::Value& jv) { vec = mImpl->convertRealVecFromStr<float>(jv.asString()); });
}

bool
InfoCodec::getVec3f(const Key& key, scene_rdl2::math::Vec3f& v3)
{
    return mImpl->get(key, [&](Json::Value& jv) { v3 = mImpl->convertRealVec3FromStr<float>(jv.asString()); });  
}

int
InfoCodec::decode(const std::string& inputData, std::function<bool()> decodeFunc)
{
    return mImpl->decode(inputData, decodeFunc);
}

bool
InfoCodec::decodeChild(const Key& childKey, std::string& childInputData)
{
    return mImpl->decodeChild(childKey, childInputData);
}

bool
InfoCodec::decodeTable(const Key& tableKey, std::string& itemKey, std::string& itemInputData)
{
    return mImpl->decodeTable(tableKey, itemKey, itemInputData);
}

std::string
InfoCodec::show() const
{
    return mImpl->show();
}

} // namespace mcrt_dataio
