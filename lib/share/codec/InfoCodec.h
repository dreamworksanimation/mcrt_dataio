// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

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

    InfoCodec(Key& infoKey, bool decodeOnly);
    ~InfoCodec();

    Key& getInfoKey() const;
    bool getDecodeOnly() const;

    void clear(); // MTsafe
    bool isEmpty(); // MTsafe

    //------------------------------

    void setBool(Key& key, const bool setVal, bool* setTarget=nullptr); // MTsafe
    void setInt(Key& key, const int setVal, int* setTarget=nullptr); // MTsafe
    void setUInt(Key& key, const unsigned int setVal, unsigned int* setTarget=nullptr); // MTsafe
    void setInt64(Key& key, const int64_t setVal, int64_t* setTarget=nullptr); // MTsafe:LongLongInt
    void setUInt64(Key& key, const uint64_t setVal, uint64_t* setTarget=nullptr); //MTsafe:LongLongUint
    void setSizeT(Key& key, const size_t setVal, size_t* setTarget=nullptr); // MTsafe
    void setFloat(Key& key, const float setVal, float* setTarget=nullptr); // MTsafe
    void setDouble(Key& key, const double setVal, double* setTarget=nullptr); // MTsafe
    void setString(Key& key, const std::string& setVal, std::string* setTarget=nullptr); // MTsafe

    void setVecFloat(Key& key, const std::vector<float>& setVal, std::vector<float>* setTarget=nullptr); // MTsafe

    bool encode(std::string& outputData); // MTsafe : true:encoded false:no-encoded-data(not error)
    void encodeChild(Key& childKey, InfoCodec& child); // MTsafe
    void encodeTable(Key& tableKey, Key& itemKey, InfoCodec& item); // MTsafe : associative array

    //------------------------------

    bool getBool(Key& key, bool& v);
    bool getInt(Key& key, int& v);
    bool getUInt(Key& key, unsigned int& v);
    bool getInt64(Key& key, int64_t& v); // long long int
    bool getUInt64(Key& key, uint64_t& v); // long long int
    bool getSizeT(Key& key, size_t& v); 
    bool getFloat(Key& key, float& v);
    bool getDouble(Key& key, double& v);
    bool getString(Key& key, std::string& v);

    bool getVecFloat(Key& key, std::vector<float>& vec);

    int  decode(const std::string& inputData, std::function<bool()> decodeFunc);

    // return true:decode-data false:not-decode-data(not-error)
    bool decodeChild(Key& childKey, std::string& childInputData);
    // return true:decode-data false:not-decode-data(not-error)
    bool decodeTable(Key& tableKey, std::string& itemKey, std::string& itemInputData);

    std::string show() const;

private:
    class Impl;
    std::unique_ptr<Impl> mImpl;
};

} // namespace mcrt_dataio
