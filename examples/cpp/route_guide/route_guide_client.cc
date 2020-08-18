/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <thread>

#include "helper.h"
#ifdef BAZEL_BUILD
#include "examples/protos/route_guide.grpc.pb.h"
#else
#include "route_guide.grpc.pb.h"
#endif

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientWriter;
using grpc::Status;
using routeguide::Feature;
using routeguide::Point;
using routeguide::Rectangle;
using routeguide::RouteGuide;
using routeguide::RouteNote;
using routeguide::RouteSummary;

class RouteGuideClient {
 public:
  RouteGuideClient(std::shared_ptr<Channel> channel, const std::string& db)
      : stub_(RouteGuide::NewStub(channel)) {
    routeguide::ParseDb(db, &feature_list_);
  }

  void RecordRoute() {
    Point point;
    RouteSummary stats;
    ClientContext context;
    const int kPoints = 10;
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();

    std::default_random_engine generator(seed);
    std::uniform_int_distribution<int> feature_distribution(
        0, feature_list_.size() - 1);
    std::uniform_int_distribution<int> delay_distribution(500, 1500);

    std::unique_ptr<ClientWriter<Point> > writer(
        stub_->RecordRoute(&context, &stats));

    int numberOfCallsToWrite = 0;
    for (int i = 0; i < kPoints; i++) {
      const Feature& f = feature_list_[feature_distribution(generator)];
      std::cout << "Visiting point " << f.location().latitude() / kCoordFactor_
                << ", " << f.location().longitude() / kCoordFactor_
                << " writer->Write()" << std::endl;

      const bool writeSucceeded = writer->Write(f.location());
      numberOfCallsToWrite++;

      if (!writeSucceeded) {
        std::cout << "Broken stream detected" << std::endl;
        break;
      }

      std::this_thread::sleep_for(
          std::chrono::milliseconds(delay_distribution(generator)));
    }

    std::cout << "Called writer->Write() " << numberOfCallsToWrite << " times"
              << std::endl;
    writer->WritesDone();

    Status status = writer->Finish();
    if (status.ok()) {
      std::cout << "writer->Finish() returned OK" << std::endl;
    } else {
      std::cout << "RecordRoute rpc failed: " << status.error_code() << ": "
                << status.error_message() << std::endl;
    }
  }

 private:
  const float kCoordFactor_ = 10000000.0;
  std::unique_ptr<RouteGuide::Stub> stub_;
  std::vector<Feature> feature_list_;
};

int main(int argc, char** argv) {
  // Expect only arg: --db_path=path/to/route_guide_db.json.
  std::string db = routeguide::GetDbFileContent(argc, argv);
  RouteGuideClient guide(
      grpc::CreateChannel("localhost:50051",
                          grpc::InsecureChannelCredentials()),
      db);

  std::cout << "-------------- RecordRoute --------------" << std::endl;
  guide.RecordRoute();

  return 0;
}
