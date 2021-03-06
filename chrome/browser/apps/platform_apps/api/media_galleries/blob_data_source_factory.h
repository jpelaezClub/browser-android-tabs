// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_API_MEDIA_GALLERIES_BLOB_DATA_SOURCE_FACTORY_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_API_MEDIA_GALLERIES_BLOB_DATA_SOURCE_FACTORY_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/services/media_gallery_util/public/cpp/safe_media_metadata_parser.h"
#include "chrome/services/media_gallery_util/public/mojom/media_parser.mojom.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace chrome_apps {
namespace api {

// Factory to provide media data source for extension media gallery API.
// Internally it will read media data from a blob in browser process.
class BlobDataSourceFactory
    : public SafeMediaMetadataParser::MediaDataSourceFactory {
 public:
  BlobDataSourceFactory(content::BrowserContext* browser_context,
                        const std::string& blob_uuid);
  ~BlobDataSourceFactory() override;

 private:
  // SafeMediaMetadataParser::MediaDataSourceFactory implementation.
  std::unique_ptr<chrome::mojom::MediaDataSource> CreateMediaDataSource(
      chrome::mojom::MediaDataSourcePtr* request,
      MediaDataCallback media_data_callback) override;

  content::BrowserContext* browser_context_;
  std::string blob_uuid_;
  MediaDataCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(BlobDataSourceFactory);
};

}  // namespace api
}  // namespace chrome_apps

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_API_MEDIA_GALLERIES_BLOB_DATA_SOURCE_FACTORY_H_
