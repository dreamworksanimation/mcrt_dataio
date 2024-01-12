// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#include "TelemetryLayout.h"
#include "TelemetryPanel.h"

#include <scene_rdl2/render/util/StrUtil.h>

#include <iomanip>
#include <sstream>

namespace mcrt_dataio {
namespace telemetry {

Panel::Panel(const std::string& panelName, LayoutBaseShPtr layout, const std::string& options)
    : mName(panelName)
    , mLayout(layout) 
    , mSetupOptions(options)
{
    parserConfigure();
    evalSetupOptions();
}

std::string
Panel::show() const
{
    using scene_rdl2::str_util::addIndent;

    std::ostringstream ostr;
    ostr << "Panel {\n"
         << "  mLayout:" << ((mLayout) ? mLayout->getName() : " -- empty --")
         << " addr:0x" << std::hex << reinterpret_cast<uintptr_t>(mLayout.get()) << '\n'
         << "  mSetupOptions:" << ((mSetupOptions.empty()) ? " -- empty --" : mSetupOptions) << '\n';
    if (mChildPanelTable) {
        ostr << addIndent("mChildPanelTable: " + mChildPanelTable->show()) << '\n';
    } else {
        ostr << "  mChildPanelTable: -- empty --\n";
    }
    ostr << "}";
    return ostr.str();
}

void
Panel::parserConfigure()
{
    mParser.description("Panel command");

    mParser.opt("layout", "...command...", "layout command",
                [&](Arg& arg) {
                    if (!mLayout) return arg.msg("mLayout is empty\n");
                    return mLayout->getParser().main(arg.childArg());
                });
    mParser.opt("show", "", "show all information",
                [&](Arg& arg) { return arg.msg(show() + '\n'); });
}

void
Panel::evalSetupOptions()
{
    if (mSetupOptions.empty()) return; // early exit
    if (!mLayout) return;              // early exit

    std::stringstream sstr { mSetupOptions };
    std::string comLine;
    while (getline(sstr, comLine, '\n')) {
        Arg arg(comLine);
        if (!mLayout->getParser().main(arg)) {
            std::cerr << "Panel::evalSetupOptions() failed. PanelName:" << getName()
                      << " Skip setup options\n";
        }
    }
}

//-----------------------------------------------------------------------------------------

int
PanelTable::findPanel(const std::string& panelName) const
{
    for (size_t id = 0; id < mTable.size(); ++id) {
        if (mTable[id]->getName() == panelName) return static_cast<int>(id);
    }
    return -1;                  // Could not find the panel
}

void
PanelTable::push_back_panel(PanelShPtr panel)
{
    mTable.push_back(panel);
}

bool
PanelTable::setCurrentPanelByName(const std::string& panelName,
                                  const std::function<void(const std::string& msg)>& msgOut)
{
    int id = find(panelName);
    if (id < 0) {
        if (msgOut) {
            std::ostringstream ostr;
            ostr << "Can not find layout. panelName:" << panelName;
            msgOut(ostr.str());
        }
        return false; // error
    }
    mCurrId = static_cast<size_t>(id);
    return true;
}

PanelTable::PanelShPtr
PanelTable::getCurrentPanel() const
{
    if (!verifyCurrIdRange()) return nullptr;
    return mTable[mCurrId];
}

PanelTable::PanelShPtr
PanelTable::getPanel(size_t panelId) const
{
    if (panelId >= mTable.size()) return nullptr;
    return mTable[panelId];
}

PanelTable::PanelShPtr
PanelTable::getPanelByName(const std::string& panelName) const
{
    int id = findPanel(panelName);
    if (id < 0) return nullptr; // Could not find panel
    return getPanel(id);
}

PanelTable::PanelShPtr
PanelTable::getLastPanel() const
{
    if (mTable.empty()) return nullptr;
    return mTable[mTable.size() - 1];
}

bool
PanelTable::currentPanelToNext()
{
    if (!mTable.size()) return false;
    if (!verifyCurrIdRange()) return false;
    ++mCurrId;
    if (!verifyCurrIdRange()) mCurrId = 0;
    return true;
}

bool
PanelTable::currentPanelToPrev()
{
    if (!mTable.size()) return false;
    if (!verifyCurrIdRange()) return false;
    if (mCurrId == 0) mCurrId = mTable.size();
    --mCurrId;
    return true;
}

void
PanelTable::getAllPanelName(std::vector<std::string>& panelNameList, const std::string& prefix)
{
    for (size_t i = 0; i < mTable.size(); ++i) {
        std::string currName = prefix + mTable[i]->getName();
        panelNameList.push_back(currName);

        PanelTableShPtr childPanelTbl = mTable[i]->getChildPanelTable();
        if (childPanelTbl) {
            childPanelTbl->getAllPanelName(panelNameList, currName + '/');
        }
    }
}

std::string
PanelTable::show() const
{
    using scene_rdl2::str_util::addIndent;
    using scene_rdl2::str_util::getNumberOfDigits;

    auto showTableItem = [&](size_t id) {
        int w = getNumberOfDigits(mTable.size());
        std::ostringstream ostr;
        ostr << "id:" << std::setw(w) << std::setfill('0') << id << " {";
        ostr << ((id == mCurrId) ? " <== current\n" : "\n");
        ostr << addIndent(mTable[id]->show()) << '\n' << "}";
        return ostr.str();
    };

    std::ostringstream ostr;
    ostr << "PanelTable {\n"
         << "  mName:" << mName << '\n'
         << "  mCurrId:" << mCurrId << '\n'
         << "  mTable (size:" << mTable.size() << ") {\n";
    for (size_t i = 0; i < mTable.size(); ++i) {
        ostr << addIndent(showTableItem(i), 2) << '\n';
    }
    ostr << "  }\n"
         << "}";
    return ostr.str();
}

void
PanelTable::parserConfigure()
{
    mParser.description("PanelTable command");

    mParser.opt("curr", "...command...", "current panel command",
                [&](Arg& arg) {
                    if (!verifyCurrIdRange()) return arg.msg("undefined current panel\n");
                    return getCurrentPanel()->getParser().main(arg.childArg());
                });
    mParser.opt("panelNameList", "", "show panel name list of this panelTable",
                [&](Arg& arg) { return arg.msg(showPanelNameList() + '\n'); });
    mParser.opt("setCurrPanelByName", "<panelName|show>", "set current panel by panelName",
                [&](Arg& arg) {
                    if (arg() == "show") arg++;
                    else {
                        setCurrentPanelByName((arg++)(),
                                              [&](const std::string& msg) { arg.msg(msg + '\n'); });
                    }
                    return arg.msg(showCurrentPanelName() + '\n');
                });
    mParser.opt("setCurrPanelNext", "", "set current panel to next",
                [&](Arg& arg) {
                    currentPanelToNext();
                    return arg.msg(showCurrentPanelName() + '\n');
                });
    mParser.opt("setCurrPanelPrev", "", "set Current panel to prev",
                [&](Arg& arg) {
                    currentPanelToPrev();
                    return arg.msg(showCurrentPanelName() + '\n');
                });
    mParser.opt("show", "", "show all info",
                [&](Arg& arg) { return arg.msg(show() + '\n'); });
}

int
PanelTable::find(const std::string& panelName) const
{
    for (size_t i = 0; i < mTable.size(); ++i) {
        if (mTable[i]->getName() == panelName) return static_cast<int>(i);
    }
    return -1; // not found
}

std::string
PanelTable::showPanelNameList() const
{
    int w = scene_rdl2::str_util::getNumberOfDigits(mTable.size() - 1);

    std::ostringstream ostr;
    ostr << "panelTable (size:" << mTable.size() << " currId:" << mCurrId << ") {\n";
    for (size_t i = 0; i < mTable.size(); ++i) {
        ostr << "  i:" << std::setw(w) << i << " panelName:" << mTable[i]->getName()
             << ((i == mCurrId) ? " <== current\n" : "\n");
    }
    ostr << "}";
    return ostr.str();
}

std::string
PanelTable::showCurrentPanelName() const
{
    std::ostringstream ostr;
    if (!verifyCurrIdRange()) return " undefined current panel";
    return mTable[mCurrId]->getName();
}

//------------------------------------------------------------------------------------------

void
PanelTableStack::init(PanelTableShPtr root)
{
    clear();
    mStack.push(root);
}

void
PanelTableStack::clear()
{
    while (!mStack.empty()) {
        mStack.pop();
    }
}

bool
PanelTableStack::setCurrentPanelByName(const std::string& panelName)
{
    if (mStack.empty()) return false;
    return mStack.top()->setCurrentPanelByName(panelName);
}

PanelTableStack::PanelShPtr
PanelTableStack::getCurrentPanel() const
{
    if (mStack.empty()) return nullptr;
    return mStack.top()->getCurrentPanel();
}

bool
PanelTableStack::currentPanelToNext() const
{
    if (mStack.empty()) return false;
    return mStack.top()->currentPanelToNext();
}

bool
PanelTableStack::currentPanelToPrev() const
{
    if (mStack.empty()) return false;
    return mStack.top()->currentPanelToPrev();
}

bool
PanelTableStack::currentPanelToParent()
{
    if (mStack.empty()) return false;
    if (mStack.size() == 1) return false; // stack is one level. can not move to parent
    mStack.pop();
    return true;
}

bool
PanelTableStack::currentPanelToChild()
{
    if (mStack.empty()) return false;
    PanelShPtr currPanel = getCurrentPanel();
    PanelTableShPtr childPanelTable = currPanel->getChildPanelTable();
    if (!childPanelTable) {
        return false;
    }
    mStack.push(childPanelTable);
    return true;
}

std::string
PanelTableStack::getCurrentPanelName() const
{
    std::stack<PanelTableShPtr> tmpStack = mStack;
    std::vector<std::string> nameTable;
    while (!tmpStack.empty()) {
        PanelShPtr currPanel = tmpStack.top()->getCurrentPanel();
        nameTable.push_back(currPanel->getName());
        tmpStack.pop();
    }

    std::ostringstream ostr;
    for (size_t i = 0; i < nameTable.size(); ++i) {
        size_t j = nameTable.size() - i - 1;
        ostr << nameTable[j];
        if (j != 0) ostr << '/';
    }
    return ostr.str();
}

void
PanelTableStack::parserConfigure()
{
    mParser.description("PanelTableStack command");

    mParser.opt("top", "...command...", "command for stack top panel table",
                [&](Arg& arg) {
                    if (mStack.empty()) return arg.msg("stack is empty\n");
                    return mStack.top()->getParser().main(arg.childArg());
                });
    mParser.opt("size", "", "show stack size",
                [&](Arg& arg) {
                    return arg.msg(std::to_string(mStack.size()) + '\n');
                });
}

} // namespace telemetry
} // namespace mcrt_dataio
