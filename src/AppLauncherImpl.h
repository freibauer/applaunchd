// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2022 Konsulko Group
 */

#ifndef APPLAUNCHER_IMPL_H
#define APPLAUNCHER_IMPL_H

#include <mutex>
#include <list>
#include <condition_variable>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include "applauncher.grpc.pb.h"
#include "systemd_manager.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerWriter;
using grpc::Status;

using automotivegradelinux::AppLauncher;
using automotivegradelinux::StartRequest;
using automotivegradelinux::StartResponse;
using automotivegradelinux::ListRequest;
using automotivegradelinux::ListResponse;
using automotivegradelinux::AppInfo;
using automotivegradelinux::StatusRequest;
using automotivegradelinux::StatusResponse;

class AppLauncherImpl final : public AppLauncher::Service
{
public:
	explicit AppLauncherImpl(SystemdManager *manager);

	Status StartApplication(ServerContext* context,
				const StartRequest* request,
				StartResponse* response) override;


	Status ListApplications(ServerContext* context,
				const ListRequest* request,
				ListResponse* response) override;

	Status GetStatusEvents(ServerContext* context,
			       const StatusRequest* request,
			       ServerWriter<StatusResponse>* writer) override;

	void SendStatus(std::string id, std::string status);

	void Shutdown() { m_done = true; m_done_cv.notify_all(); }

	static void started_cb(AppLauncherImpl *self,
			       const gchar *app_id,
			       gpointer caller) {
		if (self)
			self->HandleAppStarted(app_id);
	}

	static void terminated_cb(AppLauncherImpl *self,
				  const gchar *app_id,
				  gpointer caller) {
		if (self)
			self->HandleAppTerminated(app_id);
	}

private:
	// systemd event callback handlers
	void HandleAppStarted(std::string id);
	void HandleAppTerminated(std::string id);

	// Pointer to systemd wrapping glib object
	SystemdManager *m_manager;

	std::mutex m_clients_mutex;
	std::list<std::pair<ServerContext*, ServerWriter<StatusResponse>*> > m_clients;

	std::mutex m_done_mutex;
	std::condition_variable m_done_cv;
	bool m_done = false;

};

#endif // APPLAUNCHER_IMPL_H
