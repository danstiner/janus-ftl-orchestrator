/**
 * @file FtlConnection.cpp
 * @author Daniel Stiner (daniel.stiner@gmail.com)
 * @date 2020-11-27
 * @copyright Copyright (c) 2020 Daniel Stiner
 */

#include "ftl_orchestrator.grpc.pb.h"
#include "NodeStore.h"

#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <spdlog/spdlog.h>

using std::shared_ptr;

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::StatusCode;

using orchestrator::CreateNodeRequest;
using orchestrator::CreateNodeResponse;
using orchestrator::CreateStreamRequest;
using orchestrator::CreateStreamResponse;
using orchestrator::CreateSubscriptionRequest;
using orchestrator::CreateSubscriptionResponse;
using orchestrator::DeleteNodeRequest;
using orchestrator::DeleteNodeResponse;
using orchestrator::DeleteStreamRequest;
using orchestrator::DeleteStreamResponse;
using orchestrator::DeleteSubscriptionRequest;
using orchestrator::DeleteSubscriptionResponse;
using orchestrator::FtlOrchestrator;
using orchestrator::RegisterRelayRequest;
using orchestrator::RelayUpdate;
using orchestrator::UpdateNodeStatusRequest;
using orchestrator::UpdateNodeStatusResponse;

const Status &NODE_NOT_FOUND = Status(StatusCode::NOT_FOUND, "Node not found");

class FtlOrchestratorServiceImpl final : public FtlOrchestrator::Service
{
  Status CreateStream(ServerContext *context, const CreateStreamRequest *request, CreateStreamResponse *response) override
  {
    Node *node = nodeStore->GetNode(request->node_name());
    if (node == nullptr)
    {
      return NODE_NOT_FOUND;
    }

    node->CreateStream(request->stream_id(), request->channel_id());

    return Status::OK;
  }

  Status DeleteStream(ServerContext *context, const DeleteStreamRequest *request, DeleteStreamResponse *response) override
  {
    Node *node = nodeStore->GetNode(request->node_name());
    if (node == nullptr)
    {
      return NODE_NOT_FOUND;
    }

    node->DeleteStream(request->stream_id());

    return Status::OK;
  }
  Status CreateSubscription(ServerContext *context, const CreateSubscriptionRequest *request, CreateSubscriptionResponse *response) override
  {
    Node *node = nodeStore->GetNode(request->node_name());
    if (node == nullptr)
    {
      return NODE_NOT_FOUND;
    }

    node->CreateSubscription(request->channel_id());

    return Status::OK;
  }

  Status DeleteSubscription(ServerContext *context, const DeleteSubscriptionRequest *request, DeleteSubscriptionResponse *response) override
  {
    Node *node = nodeStore->GetNode(request->node_name());
    if (node == nullptr)
    {
      return NODE_NOT_FOUND;
    }

    node->DeleteSubscription(request->channel_id());

    return Status::OK;
  }

  Status CreateNode(ServerContext *context, const CreateNodeRequest *request, CreateNodeResponse *response) override
  {
    nodeStore->CreateNode(request->node_name());
    return Status::OK;
  }

  Status DeleteNode(ServerContext *context, const DeleteNodeRequest *request, DeleteNodeResponse *response) override
  {
    nodeStore->DeleteNode(request->node_name());
    return Status::OK;
  }

  Status UpdateNodeStatus(ServerContext *context, const UpdateNodeStatusRequest *request, UpdateNodeStatusResponse *response) override
  {
    Node *node = nodeStore->GetNode(request->node_name());
    if (node == nullptr)
    {
      return NODE_NOT_FOUND;
    }

    node->SetStatus(NodeStatus{
        .current_load = request->current_load(),
        .maximum_load = request->maximum_load(),
    });

    return Status::OK;
  }

  Status RegisterRelay(ServerContext *context, const RegisterRelayRequest *request, RelayUpdate *response) override
  {
    Node *node = nodeStore->GetNode(request->node_name());
    if (node == nullptr)
    {
      return NODE_NOT_FOUND;
    }

    node->DeleteStream(request->stream_id());

    return Status::OK;
  }

private:
  shared_ptr<NodeStore> nodeStore;
};

void RunServer()
{
  std::string server_address("0.0.0.0:50051");
  FtlOrchestratorServiceImpl service;

  grpc::EnableDefaultHealthCheckService(true);
  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

/**
 * @brief Entrypoint for the program binary.
 * 
 * @return int exit status
 */
int main()
{
  // Off we go
  RunServer();

  return 0;
}