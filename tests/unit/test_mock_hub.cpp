#include "falcon-mock-hub/mock_hub.hpp"
#include <gtest/gtest.h>

using namespace falcon::mock_hub;

TEST(MockHubTest, ServerInstantiationAndLifecycle) {
  MockHubServer server;
  EXPECT_FALSE(server.is_server_running());

  bool started = server.start(4222);
  EXPECT_TRUE(started);
  EXPECT_TRUE(server.is_server_running());
  EXPECT_EQ(server.get_port(), 4222);

  server.stop();
  EXPECT_FALSE(server.is_server_running());
}
