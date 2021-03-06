/**
 * @file FunctionalTests.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2020-11-25
 * @copyright Copyright (c) 2020 Hayden McAfee
 */

#include <catch2/catch.hpp>

#include <FtlOrchestrationClient.h>
#include <TlsConnectionManager.h>

#include <chrono>
#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>
#include <vector>

/**
 * @brief
 *  A class providing a variety of tools for managing functional testing of Orchestrator
 *  application classes.
 */
class FunctionalTestsFixture
{
public:
    FunctionalTestsFixture() : 
        preSharedKey(
            {
                std::byte(0x00), std::byte(0x01), std::byte(0x02), std::byte(0x03), 
                std::byte(0x04), std::byte(0x05), std::byte(0x06), std::byte(0x07), 
                std::byte(0x08), std::byte(0x09), std::byte(0x0a), std::byte(0x0b), 
                std::byte(0x0c), std::byte(0x0d), std::byte(0x0e), std::byte(0x0f), 
            })
    { }

    ~FunctionalTestsFixture()
    {
        if (connectionManager)
        {
            connectionManager->StopListening();
            connectionManagerListenThread.join();
        }
    }

    /**
     * @brief Instantiates a new ConnectionManager and sets up callbacks
     */
    void InitConnectionManager()
    {
        connectionManager = std::make_shared<TlsConnectionManager<FtlConnection>>(preSharedKey);
        connectionManager->SetOnNewConnection(
            std::bind(
                &FunctionalTestsFixture::onNewConnectionManagerConnection,
                this,
                std::placeholders::_1));
        connectionManager->Init();
    }

    /**
     * @brief Starts listening for connections on a separate thread
     */
    void StartConnectionManagerListening()
    {
        std::promise<void> listeningPromise;
        std::future<void> listeningFuture = listeningPromise.get_future();
        connectionManagerListenThread = std::thread(
            [this, &listeningPromise]()
            {
                this->connectionManager->Listen(std::move(listeningPromise));
            });
        listeningFuture.get();
    }

    /**
     * @brief Waits for a new connection to be reported by ConnectionManager, or times out
     * @param timeout how long to wait before timing out
     * @return std::optional<std::shared_ptr<FtlConnection>> the new connection
     */
    std::optional<std::shared_ptr<FtlConnection>> WaitForNewConnection(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(1000))
    {
        std::unique_lock<std::mutex> lock(connectionManagerMutex);
        connectionManagerConditionVariable.wait_for(lock, timeout);
        if (newConnections.size() > 0)
        {
            auto returnValue = newConnections.front();
            newConnections.pop_front();
            return returnValue;
        }
        return std::nullopt;
    }

protected:
    std::vector<std::byte> preSharedKey;
    std::mutex connectionManagerMutex;
    std::condition_variable connectionManagerConditionVariable;
    std::shared_ptr<TlsConnectionManager<FtlConnection>> connectionManager;
    std::thread connectionManagerListenThread;
    std::list<std::shared_ptr<FtlConnection>> newConnections;

    void onNewConnectionManagerConnection(std::shared_ptr<FtlConnection> connection)
    {
        {
            std::lock_guard<std::mutex> lock(connectionManagerMutex);
            newConnections.push_back(connection);
        }
        connectionManagerConditionVariable.notify_all();
    }
};

TEST_CASE_METHOD(
    FunctionalTestsFixture,
    "Orchestration client can connect to Orchestration server",
    "[functional]")
{
    InitConnectionManager();
    StartConnectionManagerListening();
    
    // Now connect a client
    std::shared_ptr<FtlConnection> clientConnection = 
        FtlOrchestrationClient::Connect("127.0.0.1", preSharedKey);

    // We need to start the client in a separate thread so we can accept the connection
    // in this thread without deadlocking.
    std::future<void> clientConnectedFuture = 
        std::async(std::launch::async, &FtlConnection::Start, clientConnection.get());

    // Make sure the server successfully received the connection
    auto receivedConnection = WaitForNewConnection();
    REQUIRE(receivedConnection.has_value());

    // Record when we see the received connection disconnect
    std::promise<void> receivedConnClosedPromise;
    std::future<void> receivedConnClosedFuture = receivedConnClosedPromise.get_future();
    receivedConnection.value()->SetOnConnectionClosed(
        [&receivedConnClosedPromise]()
        {
            receivedConnClosedPromise.set_value_at_thread_exit();
        });

    // Start the received connection
    receivedConnection.value()->Start();

    // Make sure our client finished connecting
    clientConnectedFuture.get();

    // Shut down the client
    clientConnection->Stop();

    // Wait for the received connection to close
    receivedConnClosedFuture.get();
}