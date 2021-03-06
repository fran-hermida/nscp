#pragma once

#include "session_manager_interface.hpp"
#include "helpers.hpp"

#include <client/simple_client.hpp>

#include <nscapi/nscapi_core_wrapper.hpp>

#include <RegexController.h>
#include <StreamResponse.h>

#include <boost/thread/shared_mutex.hpp>

#include <string>


class settings_controller : public Mongoose::RegexpController {
	boost::shared_ptr<session_manager_interface> session;
	const nscapi::core_wrapper* core;
	const unsigned int plugin_id;

public:

	settings_controller(boost::shared_ptr<session_manager_interface> session, nscapi::core_wrapper* core, unsigned int plugin_id);

	void get_key(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
	void get_section(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
};
