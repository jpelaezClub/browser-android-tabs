// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_operation_runner.h"
#include "chrome/browser/chromeos/file_manager/documents_provider_root_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/arc_features.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_file_system_instance.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {

namespace {

class TestObserver : public DocumentsProviderRootManager::Observer {
 public:
  // DocumentsProviderRootManager::Observer overrides:
  void OnDocumentsProviderRootAdded(
      const std::string& authority,
      const std::string& root_id,
      const std::string& document_id,
      const std::string& title,
      const std::string& summary,
      const GURL& icon_url,
      bool read_only,
      const std::vector<std::string>& mime_types) override {
    added_authorities_.push_back(authority);
  }
  void OnDocumentsProviderRootRemoved(const std::string& authority,
                                      const std::string& root_id,
                                      const std::string& document_id) override {
    removed_authorities_.push_back(authority);
  }

  void Reset() {
    added_authorities_.clear();
    removed_authorities_.clear();
  }
  std::vector<std::string> added_authorities() { return added_authorities_; }
  std::vector<std::string> removed_authorities() {
    return removed_authorities_;
  }

 private:
  std::vector<std::string> added_authorities_;
  std::vector<std::string> removed_authorities_;
};

}  // namespace

class DocumentsProviderRootManagerTest : public testing::Test {
 public:
  DocumentsProviderRootManagerTest() = default;
  ~DocumentsProviderRootManagerTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        arc::kEnableDocumentsProviderInFilesAppFeature);
    arc_service_manager_ = std::make_unique<arc::ArcServiceManager>();
    profile_ = std::make_unique<TestingProfile>();
    arc::ArcFileSystemBridge::GetForBrowserContextForTesting(profile_.get());
    runner_ = arc::ArcFileSystemOperationRunner::CreateForTesting(
        profile_.get(), arc_service_manager_->arc_bridge_service());
    root_manager_ = std::make_unique<DocumentsProviderRootManager>(
        profile_.get(), runner_.get());
    root_manager_->AddObserver(&observer_);
    arc_service_manager_->arc_bridge_service()->file_system()->SetInstance(
        &file_system_instance_);
    WaitForInstanceReady(
        arc_service_manager_->arc_bridge_service()->file_system());
    ASSERT_TRUE(file_system_instance_.InitCalled());
  }

  void TearDown() override {
    root_manager_->RemoveObserver(&observer_);
    root_manager_.reset();
    arc_service_manager_->arc_bridge_service()->file_system()->CloseInstance(
        &file_system_instance_);
    if (runner_)
      runner_->Shutdown();
  }

  void AddFakeRoot(const std::string& authority,
                   const std::string& root_id,
                   const std::string& document_id,
                   const std::string& title) {
    file_system_instance_.AddRoot(arc::FakeFileSystemInstance::Root(
        authority, root_id, document_id, title));
  }

  TestObserver& observer() { return observer_; }

 protected:
  std::unique_ptr<DocumentsProviderRootManager> root_manager_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  arc::FakeFileSystemInstance file_system_instance_;
  std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<arc::ArcFileSystemOperationRunner> runner_;
  TestObserver observer_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(DocumentsProviderRootManagerTest);
};

TEST_F(DocumentsProviderRootManagerTest, AddMultipleRoots) {
  AddFakeRoot("authority1", "", "", "");
  AddFakeRoot("authority2", "", "", "");
  root_manager_->SetEnabled(true);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2u, observer().added_authorities().size());
  EXPECT_EQ(0u, observer().removed_authorities().size());
  EXPECT_EQ("authority1", observer().added_authorities()[0]);
  EXPECT_EQ("authority2", observer().added_authorities()[1]);
}

TEST_F(DocumentsProviderRootManagerTest, ExcludeBlacklistedRoots) {
  AddFakeRoot("authority1", "", "", "");
  AddFakeRoot("com.android.externalstorage.documents", "", "", "");
  AddFakeRoot("com.android.providers.downloads.documents", "", "", "");
  AddFakeRoot("com.android.providers.media.documents", "", "", "");
  AddFakeRoot("com.google.android.apps.docs.storage", "", "", "");
  root_manager_->SetEnabled(true);
  base::RunLoop().RunUntilIdle();

  // Only authority1 should be notified and all other authorities should be
  // ignored.
  EXPECT_EQ(1u, observer().added_authorities().size());
  EXPECT_EQ(0u, observer().removed_authorities().size());
}

TEST_F(DocumentsProviderRootManagerTest, DisableRootManager) {
  AddFakeRoot("authority1", "", "", "");
  AddFakeRoot("authority2", "", "", "");
  root_manager_->SetEnabled(true);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2u, observer().added_authorities().size());
  EXPECT_EQ(0u, observer().removed_authorities().size());
  observer().Reset();

  // When disabled, OnDocumentsProviderRootRemoved() should be called for each
  // existing root to clear them from the Files app UI.
  root_manager_->SetEnabled(false);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, observer().added_authorities().size());
  EXPECT_EQ(2u, observer().removed_authorities().size());
}

TEST_F(DocumentsProviderRootManagerTest, DoNotNotifyUnchangedRoot) {
  AddFakeRoot("authority1", "", "", "");
  AddFakeRoot("authority2", "", "", "");
  root_manager_->SetEnabled(true);
  base::RunLoop().RunUntilIdle();

  // Root list is now ["authority1", "authority2"].
  EXPECT_EQ(2u, observer().added_authorities().size());
  EXPECT_EQ(0u, observer().removed_authorities().size());
  observer().Reset();

  AddFakeRoot("authority3", "", "", "");
  root_manager_->OnRootsChanged();
  base::RunLoop().RunUntilIdle();

  // Root list is now ["authority1", "authority2", "authority3"].
  // Only authority3 should be notified by the OnRootsChanged() above.
  // (authority1, authority2 should not be notified multiple times)
  EXPECT_EQ(1u, observer().added_authorities().size());
  EXPECT_EQ("authority3", observer().added_authorities()[0]);
  EXPECT_EQ(0u, observer().removed_authorities().size());
}

}  // namespace file_manager
