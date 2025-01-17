// Copyright 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////////

#include "tink/mac/mac_config.h"

#include "tink/catalogue.h"
#include "tink/config.h"
#include "tink/keyset_handle.h"
#include "tink/mac.h"
#include "tink/mac/mac_key_templates.h"
#include "tink/registry.h"
#include "tink/util/status.h"
#include "gtest/gtest.h"
#include "tink/util/test_util.h"

namespace crypto {
namespace tink {
namespace {

using ::crypto::tink::test::DummyMac;

class DummyMacCatalogue : public Catalogue<Mac> {
 public:
  DummyMacCatalogue() {}

  crypto::tink::util::StatusOr<std::unique_ptr<KeyManager<Mac>>>
  GetKeyManager(const std::string& type_url,
                const std::string& primitive_name,
                uint32_t min_version) const override {
    return util::Status::UNKNOWN;
  }
};


class MacConfigTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Registry::Reset();
  }
};

TEST_F(MacConfigTest, testBasic) {
  std::string hmac_key_type = "type.googleapis.com/google.crypto.tink.HmacKey";
  std::string aes_cmac_key_type =
      "type.googleapis.com/google.crypto.tink.AesCmacKey";
  auto& config = MacConfig::Latest();

  EXPECT_EQ(2, MacConfig::Latest().entry_size());
  EXPECT_EQ("TinkMac", config.entry(0).catalogue_name());
  EXPECT_EQ("Mac", config.entry(0).primitive_name());
  EXPECT_EQ(hmac_key_type, config.entry(0).type_url());
  EXPECT_EQ(true, config.entry(0).new_key_allowed());
  EXPECT_EQ(0, config.entry(0).key_manager_version());

  EXPECT_EQ("TinkMac", config.entry(1).catalogue_name());
  EXPECT_EQ("Mac", config.entry(1).primitive_name());
  EXPECT_EQ(aes_cmac_key_type, config.entry(1).type_url());
  EXPECT_EQ(true, config.entry(1).new_key_allowed());
  EXPECT_EQ(0, config.entry(1).key_manager_version());

  // No key manager before registration.
  auto manager_result = Registry::get_key_manager<Mac>(hmac_key_type);
  EXPECT_FALSE(manager_result.ok());
  EXPECT_EQ(util::error::NOT_FOUND, manager_result.status().error_code());

  // No key manager before registration.
  manager_result = Registry::get_key_manager<Mac>(aes_cmac_key_type);
  EXPECT_FALSE(manager_result.ok());
  EXPECT_EQ(util::error::NOT_FOUND, manager_result.status().error_code());

  // Registration of standard key types works.
  auto status = MacConfig::Register();
  EXPECT_TRUE(status.ok()) << status;
  manager_result = Registry::get_key_manager<Mac>(hmac_key_type);
  EXPECT_TRUE(manager_result.ok()) << manager_result.status();
  EXPECT_TRUE(manager_result.ValueOrDie()->DoesSupport(hmac_key_type));
  manager_result = Registry::get_key_manager<Mac>(aes_cmac_key_type);
  EXPECT_TRUE(manager_result.ok()) << manager_result.status();
  EXPECT_TRUE(manager_result.ValueOrDie()->DoesSupport(aes_cmac_key_type));
}

TEST_F(MacConfigTest, testRegister) {
  std::string key_type = "type.googleapis.com/google.crypto.tink.HmacKey";

  // Try on empty registry.
  auto status = Config::Register(MacConfig::Latest());
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(util::error::NOT_FOUND, status.error_code());
  auto manager_result = Registry::get_key_manager<Mac>(key_type);
  EXPECT_FALSE(manager_result.ok());

  // Register and try again.
  status = MacConfig::Register();
  EXPECT_TRUE(status.ok()) << status;
  manager_result = Registry::get_key_manager<Mac>(key_type);
  EXPECT_TRUE(manager_result.ok()) << manager_result.status();

  // Try Register() again, should succeed (idempotence).
  status = MacConfig::Register();
  EXPECT_TRUE(status.ok()) << status;

  // Reset the registry, and try overriding a catalogue with a different one.
  Registry::Reset();
  status =
      Registry::AddCatalogue("TinkMac", absl::make_unique<DummyMacCatalogue>());
  EXPECT_TRUE(status.ok()) << status;
  status = MacConfig::Register();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(util::error::ALREADY_EXISTS, status.error_code());
}

// Tests that the MacWrapper has been properly registered and we can wrap
// primitives.
TEST_F(MacConfigTest, WrappersRegistered) {
  ASSERT_TRUE(MacConfig::Register().ok());

  google::crypto::tink::Keyset::Key key;
  key.set_status(google::crypto::tink::KeyStatusType::ENABLED);
  key.set_key_id(1234);
  key.set_output_prefix_type(google::crypto::tink::OutputPrefixType::RAW);
  auto primitive_set = absl::make_unique<PrimitiveSet<Mac>>();
  primitive_set->set_primary(
      primitive_set->AddPrimitive(absl::make_unique<DummyMac>("dummy"), key)
          .ValueOrDie());

  auto primitive_result = Registry::Wrap(std::move(primitive_set));

  ASSERT_TRUE(primitive_result.ok()) << primitive_result.status();
  auto mac_result =
      primitive_result.ValueOrDie()->ComputeMac("verified text");
  ASSERT_TRUE(mac_result.ok());

  EXPECT_TRUE(DummyMac("dummy")
                  .VerifyMac(mac_result.ValueOrDie(), "verified text")
                  .ok());
  EXPECT_FALSE(
      DummyMac("dummy").VerifyMac(mac_result.ValueOrDie(), "faked text").ok());
}

}  // namespace
}  // namespace tink
}  // namespace crypto
