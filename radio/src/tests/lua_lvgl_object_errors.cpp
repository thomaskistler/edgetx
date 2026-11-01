/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   opentx - https://github.com/opentx/opentx
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "gtests.h"

#if defined(LUA) && defined(COLORLCD)

#include "edgetx.h"
#include "lua/api_colorlcd.h"
#include "lua/lua_widget.h"
#include "mainwindow.h"
#include "view_main.h"

extern void lv_stb_init();
extern int custom_lua_atpanic(lua_State* L);

namespace
{

class ErrorScriptManager : public LuaScriptManager
{
 public:
  Window* root = nullptr;
  lua_State* L = nullptr;

  Window* getCurrentParent() const override
  {
    return (tempParent && tempParent->getWindow()) ? tempParent->getWindow()
                                                    : root;
  }
  void clear() override
  {
    if (L) clearRefs(L);
    root->clear();
  }
  bool useLvglLayout() const override { return true; }
  bool isAppMode() const override { return false; }
  bool isWidget() override { return true; }
  bool isFullscreen() override { return true; }
  void luaShowError() override { errors += 1; }
  int errors = 0;
};

class LuaLvglObjectErrors : public ::testing::Test
{
 protected:
  lua_State* L = nullptr;
  LuaScriptManager* previousManager = nullptr;
  our_longjmp* previousPanicChain = nullptr;
  ErrorScriptManager manager;

  static void SetUpTestSuite() { lv_stb_init(); }

  void SetUp() override
  {
    previousPanicChain = global_lj;
    L = luaL_newstate();
    ASSERT_NE(L, nullptr);
    lua_atpanic(L, &custom_lua_atpanic);
    luaRegisterLibraries(L);
    manager.root = new Window(nullptr, {0, 0, 200, 200});
    manager.L = L;
    previousManager = luaScriptManager;
    luaScriptManager = &manager;
  }

  void TearDown() override
  {
    // Isolate the shared callback helper's pre-existing panic-chain leak.
    global_lj = previousPanicChain;
    EXPECT_EQ(manager.errors, 0);
    manager.clearRefs(L);
    manager.root->closeWindow();
    MainWindow::instance()->run();
    lua_close(L);
    luaScriptManager = previousManager;
  }

  ::testing::AssertionResult exec(const std::string& code)
  {
    if (luaL_dostring(L, code.c_str()))
      return ::testing::AssertionFailure() << lua_tostring(L, -1);
    return ::testing::AssertionSuccess();
  }
};

TEST_F(LuaLvglObjectErrors, FailedConstructorRestoresTheParent)
{
  ASSERT_TRUE(exec("box=lvgl.box({x=0,y=0,w=100,h=100});"
                   "assert(not pcall(lvgl.label,box,{bogus=1}));"
                   "top=lvgl.box({x=0,y=0,w=10,h=10})"));
  lua_getglobal(L, "box");
  auto boxWindow = LvglWidgetObjectBase::checkLvgl(L, -1, true)->getWindow();
  lua_pop(L, 1);
  lua_getglobal(L, "top");
  auto topWindow = LvglWidgetObjectBase::checkLvgl(L, -1, true)->getWindow();
  lua_pop(L, 1);
  EXPECT_NE(topWindow->getParent(), boxWindow);
  EXPECT_EQ(topWindow->getParent(), manager.root);
}

TEST_F(LuaLvglObjectErrors, FailedChildrenLeaveNoWindowBehind)
{
  ASSERT_TRUE(exec("box=lvgl.box({x=0,y=0,w=100,h=100});"
                   "assert(not pcall(lvgl.box,box,{x=0,y=0,w=10,h=10,"
                   "children={{type='bogus'}}}));"
                   "other=lvgl.box({x=0,y=0,w=10,h=10})"));
  lua_getglobal(L, "box");
  auto boxWindow = LvglWidgetObjectBase::checkLvgl(L, -1, true)->getWindow();
  lua_pop(L, 1);
  MainWindow::instance()->run();
  EXPECT_EQ(boxWindow->getChildren().size(), 0u);
  lua_getglobal(L, "other");
  auto other = LvglWidgetObjectBase::checkLvgl(L, -1, true)->getWindow();
  lua_pop(L, 1);
  EXPECT_EQ(other->getParent(), manager.root);
}

TEST_F(LuaLvglObjectErrors, FailedLineCallbacksLeaveNoLvglObjects)
{
  ASSERT_TRUE(exec("assert(not pcall(lvgl.line,{"
                   "pts={{0,0},{10,10}},pos=function() return {},0 end}))"));
  EXPECT_EQ(lv_obj_get_child_cnt(manager.root->getLvObj()), 0u);
}

TEST_F(LuaLvglObjectErrors, FailedLabelCallbacksLeaveNoLvglObjects)
{
  ASSERT_TRUE(exec("assert(not pcall(lvgl.label,{"
                   "text=function() return {} end}))"));
  EXPECT_EQ(lv_obj_get_child_cnt(manager.root->getLvObj()), 0u);
}

TEST_F(LuaLvglObjectErrors, FailedBuildRestoresTheParent)
{
  ASSERT_TRUE(exec("box=lvgl.box({x=0,y=0,w=100,h=100});"
                   "assert(not pcall(lvgl.build,box,{{type='bogus'}}));"
                   "top=lvgl.box({x=0,y=0,w=10,h=10})"));
  lua_getglobal(L, "box");
  auto boxWindow = LvglWidgetObjectBase::checkLvgl(L, -1, true)->getWindow();
  lua_pop(L, 1);
  lua_getglobal(L, "top");
  auto topWindow = LvglWidgetObjectBase::checkLvgl(L, -1, true)->getWindow();
  lua_pop(L, 1);
  EXPECT_EQ(topWindow->getParent(), manager.root);
  EXPECT_NE(topWindow->getParent(), boxWindow);
}

TEST_F(LuaLvglObjectErrors, ConstructorErrorsKeepTheScriptLocation)
{
  ASSERT_TRUE(exec("local ok, msg = pcall(function() lvgl.label({bogus=1}) end)\n"
                   "assert(not ok)\n"
                   "assert(string.find(msg, 'Invalid property', 1, true), msg)\n"
                   "assert(string.find(msg, '^%[string.-%]:1: '), msg)"));
}

TEST_F(LuaLvglObjectErrors, SuccessfulCreationStillReturnsTheObject)
{
  ASSERT_TRUE(exec("box=lvgl.box({x=0,y=0,w=100,h=100});"
                   "lbl=box:label({x=1,y=1,text='x'}); assert(lbl ~= nil);"
                   "lbl:set({text='y'});"
                   "local t=lvgl.build(box,{{type=lvgl.BOX,name='b',w=10,h=10,"
                   "children={{type=lvgl.LABEL,name='c',text='z'}}}});"
                   "assert(t.b ~= nil)"));
}

}  // namespace

#endif
