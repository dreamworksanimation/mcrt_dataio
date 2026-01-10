// Copyright 2023-2025 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <scene_rdl2/common/math/Vec3.h>

#include <memory>
#include <functional>           // function
#include <string>
#include <vector>

namespace mcrt_dataio {

class InfoCodec
//
// This class provides the functionality of encode and decode free format information
// for message passing. Using this class, we can send/receive information in a very
// flexible way between different nodes. Information itself is free format and you can
// design your own data structure by yourself using set* functions.
// Internally this class heavily depends on the JSON format and all the information is
// encoded as ASCII data JSON format. This means all the encoded data is ASCII.
// Sometimes this ASCII data format has issues regarding the precision of float format.
// You have to keep in mind. In order to solve this issue, one of the idea is MOONRAY-4202
//
// Basically all the information is represented by multiple sets of pairs of JSON-key and
// value. In order to decode the data, the decode data has to know all the possible key
// strings inside target encoded data. 
// This class never encode key table information for all internal data.
// This intended that encode side program and decode side program should be the same
// version of the source code. And decode side logic should have hardcoded all possible
// information key strings internally.
//
// Basically all the functionalities are working for information send/receive between
// different nodes. However this class has some message size issues due to ASCII format
// internally. We might consider message size when using this class to send huge data.
//
// This class is used in order to send and receive miscellaneous small but high 
// frequency information between backend progmcrt computation to client via merge
// computation.
//
{
public:    
    using Key = const std::string;

    InfoCodec(const Key& infoKey, const bool decodeOnly);
    ~InfoCodec();

    Key& getInfoKey() const;
    bool getDecodeOnly() const;

    void clear(); // MTsafe
    bool isEmpty(); // MTsafe

    //------------------------------

    void setBool(const Key& key, const bool setVal, bool* setTarget=nullptr); // MTsafe
    void setInt(const Key& key, const int setVal, int* setTarget=nullptr); // MTsafe
    void setUInt(const Key& key, const unsigned int setVal, unsigned int* setTarget=nullptr); // MTsafe
    void setInt64(const Key& key, const int64_t setVal, int64_t* setTarget=nullptr); // MTsafe:LongLongInt
    void setUInt64(const Key& key, const uint64_t setVal, uint64_t* setTarget=nullptr); //MTsafe:LongLongUint
    void setSizeT(const Key& key, const size_t setVal, size_t* setTarget=nullptr); // MTsafe
    void setFloat(const Key& key, const float setVal, float* setTarget=nullptr); // MTsafe
    void setDouble(const Key& key, const double setVal, double* setTarget=nullptr); // MTsafe
    void setString(const Key& key, const std::string& setVal, std::string* setTarget=nullptr); // MTsafe

    void setVecFloat(Key& key, const std::vector<float>& setVal, std::vector<float>* setTarget=nullptr); // MTsafe
    void setVec3f(Key& key, const scene_rdl2::math::Vec3f& setVal, scene_rdl2::math::Vec3f* setTarget=nullptr); // MTsafe

    bool encode(std::string& outputData); // MTsafe : true:encoded false:no-encoded-data(not error)
    void encodeChild(const Key& childKey, InfoCodec& child); // MTsafe
    void encodeTable(const Key& tableKey, const Key& itemKey, InfoCodec& item); // MTsafe : associative array

    //------------------------------

    bool getBool(const Key& key, bool& v);
    bool getInt(const Key& key, int& v);
    bool getUInt(const Key& key, unsigned int& v);
    bool getInt64(const Key& key, int64_t& v); // long long int
    bool getUInt64(const Key& key, uint64_t& v); // long long int
    bool getSizeT(const Key& key, size_t& v); 
    bool getFloat(const Key& key, float& v);
    bool getDouble(const Key& key, double& v);
    bool getString(const Key& key, std::string& v);

    bool getVecFloat(const Key& key, std::vector<float>& vec);
    bool getVec3f(const Key& key, scene_rdl2::math::Vec3f& v3);

    int  decode(const std::string& inputData, std::function<bool()> decodeFunc);

    // return true:decode-data false:not-decode-data(not-error)
    bool decodeChild(const Key& childKey, std::string& childInputData);
    // return true:decode-data false:not-decode-data(not-error)
    bool decodeTable(const Key& tableKey, std::string& itemKey, std::string& itemInputData);

    std::string show() const;

private:
    class Impl;
    std::unique_ptr<Impl> mImpl;
};

} // namespace mcrt_dataio
