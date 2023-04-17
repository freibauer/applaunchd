// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2022 Konsulko Group
 */

#include <AppLauncherImpl.h>
#include <systemd_manager.h>

using grpc::StatusCode;
using automotivegradelinux::AppStatus;


AppLauncherImpl::AppLauncherImpl(SystemdManager *manager) :
	m_manager(manager)
{
	systemd_manager_connect_callbacks(m_manager,
					  G_CALLBACK(started_cb),
					  G_CALLBACK(terminated_cb),
					  this);
}

Status AppLauncherImpl::StartApplication(ServerContext* context,
					 const StartRequest* request,
					 StartResponse* response)
{
	if (!m_manager)
		return Status(StatusCode::INTERNAL, "Initialization failed");

	// Search the apps list for the given app-id
	std::string app_id = request->id();
	auto dbus_launcher_info = systemd_manager_get_app_info(m_manager, app_id.c_str());
	if (!dbus_launcher_info) {
		std::string error("Unknown application '");
		error += app_id;
		error += "'";
		return Status(StatusCode::INVALID_ARGUMENT, error);
	}

	gboolean status = systemd_manager_start_app(m_manager, dbus_launcher_info);
	response->set_status(status);
	if (!status) {
		// Maybe just return StatusCode::NOT_FOUND instead?
		std::string error("Failed to start application '");
		error += app_id;
		error += "'";
		response->set_message(error);
	}

	return Status::OK;
}

Status AppLauncherImpl::ListApplications(ServerContext* context,
					 const ListRequest* request,
					 ListResponse* response)
{
	if (!m_manager)
		return Status(StatusCode::INTERNAL, "Initialization failed");

	GList *apps = systemd_manager_get_app_list(m_manager);
	if (!apps)
		return Status::OK; // Perhaps return failure here?

	guint len = g_list_length(apps);
	for (guint i = 0; i < len; i++) {
		struct _AppInfo *app_info = (struct _AppInfo*) g_list_nth_data(apps, i);
		auto info = response->add_apps();
		info->set_id(app_info_get_app_id(app_info));
		info->set_name(app_info_get_name(app_info));
		info->set_icon_path(app_info_get_icon_path(app_info));
	}

	return Status::OK;
}

Status AppLauncherImpl::GetStatusEvents(ServerContext* context,
					const StatusRequest* request,
					ServerWriter<StatusResponse>* writer)
{

	// Save client information
	m_clients_mutex.lock();
	m_clients.push_back(std::pair(context, writer));
	m_clients_mutex.unlock();

	// For now block until client disconnect / server shutdown
	// A switch to the async or callback server APIs might be more elegant than
        // holding the thread like this, and may be worth investigating at some point.
	std::unique_lock lock(m_done_mutex);
	m_done_cv.wait(lock, [context, this]{ return (context->IsCancelled() || m_done); });

	return Status::OK;
}


void AppLauncherImpl::SendStatus(std::string id, std::string status)
{
	const std::lock_guard<std::mutex> lock(m_clients_mutex);

	if (m_clients.empty())
		return;

	StatusResponse response;
	auto app_status = response.mutable_app();
	app_status->set_id(id);
	app_status->set_status(status);

	auto it = m_clients.begin();
	while (it != m_clients.end()) {
		if (it->first->IsCancelled()) {
			// Client has gone away, remove from list
			std::cout << "Removing cancelled RPC client!" << std::endl;
			it = m_clients.erase(it);

			// We're not exiting, but wake up blocked client RPC handlers so
			// the canceled one will clean exit.
			// Note that in practice this means the client RPC handler thread
			// sticks around until the next status event is sent.
			m_done_cv.notify_all();

			continue;
		} else {
			it->second->Write(response);
			++it;
		}
	}
}

void AppLauncherImpl::HandleAppStarted(std::string id)
{
	SendStatus(id, "started");
}

void AppLauncherImpl::HandleAppTerminated(std::string id)
{
	SendStatus(id, "terminated");
}

