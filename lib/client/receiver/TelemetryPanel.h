// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <scene_rdl2/common/grid_util/Parser.h>

#include <memory>
#include <stack>
#include <string>
#include <vector>

namespace mcrt_dataio {
namespace telemetry {

class LayoutBase;
class PanelTable;

class Panel
{
public:
    using Arg = scene_rdl2::grid_util::Arg;
    using LayoutBaseShPtr = std::shared_ptr<LayoutBase>;
    using PanelTableShPtr = std::shared_ptr<PanelTable>;
    using Parser = scene_rdl2::grid_util::Parser;

    // You can specify multiple-lines options strings each line is separated by '\n'
    // and each line is evaluated separately by sequential.
    Panel(const std::string& panelName,
          LayoutBaseShPtr layout,
          const std::string& options);

    const std::string& getName() const { return mName; }
    LayoutBaseShPtr getLayout() const { return mLayout; }

    void setChildPanelTable(PanelTableShPtr childPanelTable) { mChildPanelTable = childPanelTable; }
    PanelTableShPtr getChildPanelTable() const { return mChildPanelTable; }

    std::string show() const;

    Parser& getParser() { return mParser; }    

private:
    void parserConfigure();
    void evalSetupOptions();

    std::string mName;

    LayoutBaseShPtr mLayout {nullptr};
    std::string mSetupOptions;

    PanelTableShPtr mChildPanelTable {nullptr};

    Parser mParser;
};

class PanelTable
{
public:
    using Arg = scene_rdl2::grid_util::Arg;
    using PanelShPtr = std::shared_ptr<Panel>;
    using PanelTableShPtr = std::shared_ptr<PanelTable>;
    using Parser = scene_rdl2::grid_util::Parser;

    PanelTable(const std::string& name)
        : mName(name)
    {
        parserConfigure();
    }

    const std::string& getPanelTableName() const { return mName; }

    void setCurrId(size_t id) { mCurrId = id; }
    int findPanel(const std::string& panelName) const; // -1 : can not find the panel

    void push_back_panel(PanelShPtr panel);

    bool setCurrentPanelByName(const std::string& panelName,
                               const std::function<void(const std::string& msg)>& msgOut = nullptr);
    PanelShPtr getCurrentPanel() const;
    PanelShPtr getPanel(size_t panelId) const;
    PanelShPtr getPanelByName(const std::string& panelName) const;
    PanelShPtr getLastPanel() const;
    bool currentPanelToNext();
    bool currentPanelToPrev();

    void getAllPanelName(std::vector<std::string>& panelNameList, const std::string& prefix);

    std::string show() const;

    Parser& getParser() { return mParser; }    

private:
    void parserConfigure();

    bool verifyCurrIdRange() const { return mCurrId < mTable.size(); }
    int find(const std::string& panelName) const; // -1 : can not find the panel

    std::string showPanelNameList() const;
    std::string showCurrentPanelName() const;

    //------------------------------
    
    std::string mName;

    size_t mCurrId {0};
    std::vector<PanelShPtr> mTable;

    Parser mParser;
};

class PanelTableStack
{
public:
    using Arg = scene_rdl2::grid_util::Arg;
    using PanelShPtr = std::shared_ptr<Panel>;
    using PanelTableShPtr = std::shared_ptr<PanelTable>;
    using Parser = scene_rdl2::grid_util::Parser;
    
    PanelTableStack()
    {
        parserConfigure();
    }

    void init(PanelTableShPtr root);
    void clear();

    bool setCurrentPanelByName(const std::string& panelName);
    PanelShPtr getCurrentPanel() const;
    bool currentPanelToNext() const;
    bool currentPanelToPrev() const;
    bool currentPanelToParent();
    bool currentPanelToChild();
    
    std::string getCurrentPanelName() const;

    Parser& getParser() { return mParser; }

private:
    void parserConfigure();

    //------------------------------

    std::stack<PanelTableShPtr> mStack;

    Parser mParser;    
};

} // namespace telemetry
} // namespace mcrt_dataio
