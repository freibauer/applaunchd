// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2022 Konsulko Group
 */

#include <thread>
#include <chrono>
#include <glib.h>
#include <glib-unix.h>

#include "systemd_manager.h"
#include "AppLauncherImpl.h"

GMainLoop *main_loop = NULL;

AppLauncherImpl *g_service = NULL;

static gboolean quit_cb(gpointer user_data)
{
    g_info("Quitting...");

    if (main_loop)
        g_idle_add(G_SOURCE_FUNC(g_main_loop_quit), main_loop);
    else
        exit(0);

    return G_SOURCE_REMOVE;
}

void RunGrpcServer(std::shared_ptr<Server> &server)
{
    // Start server and wait for shutdown
    server->Wait();
}

int main(int argc, char *argv[])
{
    main_loop = g_main_loop_new(NULL, FALSE);

    SystemdManager *manager = systemd_manager_get_default();

    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    ServerBuilder builder;

    // Listen on the given address without any authentication mechanism (for now)
    std::string server_address("localhost:50052");
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

    // Register "service" as the instance through which we'll communicate with
    // clients. In this case it corresponds to a *synchronous* service.
    AppLauncherImpl *service = new AppLauncherImpl(manager);
    builder.RegisterService(service);

    // Finally assemble the server.
    std::shared_ptr<Server> server(builder.BuildAndStart());
    if (!server) {
        exit(1);
    }
    std::cout << "Server listening on " << server_address << std::endl;

    g_unix_signal_add(SIGTERM, quit_cb, (gpointer) &server);
    g_unix_signal_add(SIGINT, quit_cb, (gpointer) &server);

    // Start gRPC API server on its own thread
    std::thread grpc_thread(RunGrpcServer, std::ref(server));

    g_main_loop_run(main_loop);

    // Service implementation may have threads blocked from client streaming
    // RPCs, make sure those exit.
    service->Shutdown();

    // Need to set a deadline to avoid blocking on clients doing streaming
    // RPC reads
    server->Shutdown(std::chrono::system_clock::now() + std::chrono::milliseconds(500));

    grpc_thread.join();

    g_object_unref(manager);

    g_main_loop_unref(main_loop);

    return 0;
}
