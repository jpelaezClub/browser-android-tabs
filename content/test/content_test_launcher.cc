// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_launcher.h"

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/process/memory.h"
#include "base/system/sys_info.h"
#include "base/test/launcher/test_launcher.h"
#include "base/test/test_suite.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_test_suite_base.h"
#include "content/shell/app/shell_main_delegate.h"
#include "content/shell/common/shell_switches.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/buildflags.h"
#include "ui/base/ui_base_switches.h"

namespace content {

class ContentBrowserTestSuite : public ContentTestSuiteBase {
 public:
  ContentBrowserTestSuite(int argc, char** argv)
      : ContentTestSuiteBase(argc, argv) {
  }
  ~ContentBrowserTestSuite() override {}

 protected:
  void Initialize() override {
    // Browser tests are expected not to tear-down various globals. (Must run
    // before the base class is initialized.)
    base::TestSuite::DisableCheckForLeakedGlobals();

    ContentTestSuiteBase::Initialize();

#if defined(OS_ANDROID)
    RegisterInProcessThreads();
#endif
  }

  DISALLOW_COPY_AND_ASSIGN(ContentBrowserTestSuite);
};

class ContentTestLauncherDelegate : public TestLauncherDelegate {
 public:
  ContentTestLauncherDelegate() {}
  ~ContentTestLauncherDelegate() override {}

  int RunTestSuite(int argc, char** argv) override {
    return ContentBrowserTestSuite(argc, argv).Run();
  }

  bool AdjustChildProcessCommandLine(
      base::CommandLine* command_line,
      const base::FilePath& temp_data_dir) override {
    command_line->AppendSwitchPath(switches::kContentShellDataPath,
                                   temp_data_dir);
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
    return true;
  }

 protected:
#if !defined(OS_ANDROID)
  ContentMainDelegate* CreateContentMainDelegate() override {
    return new ShellMainDelegate(true);
  }
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentTestLauncherDelegate);
};

}  // namespace content

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  size_t parallel_jobs = base::NumParallelJobs();
  if (parallel_jobs > 1U) {
    parallel_jobs /= 2U;
  }
  content::ContentTestLauncherDelegate launcher_delegate;
  return LaunchTests(&launcher_delegate, parallel_jobs, argc, argv);
}
