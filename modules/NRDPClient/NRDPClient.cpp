/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "NRDPClient.h"

#include "nrdp_client.hpp"
#include "nrdp_handler.hpp"

#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>

#include <str/utils.hpp>
#include <http/client.hpp>

#include <boost/make_shared.hpp>


/**
 * Default c-tor
 * @return
 */
NRDPClient::NRDPClient() : client_("nrdp", boost::make_shared<nrdp_client::nrdp_client_handler>(), boost::make_shared<nrdp_handler::options_reader_impl>()) {}

/**
 * Default d-tor
 * @return
 */
NRDPClient::~NRDPClient() {}

bool NRDPClient::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
	try {
		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias("NRDP", alias, "client");
		client_.set_path(settings.alias().get_settings_path("targets"));

		settings.alias().add_path_to_settings()
			("SMTP CLIENT SECTION", "Section for SMTP passive check module.")
			("handlers", sh::fun_values_path(boost::bind(&NRDPClient::add_command, this, _1, _2)),
				"CLIENT HANDLER SECTION", "",
				"CLIENT HANDLER", "For more configuration options add a dedicated section")

			("targets", sh::fun_values_path(boost::bind(&NRDPClient::add_target, this, _1, _2)),
				"REMOTE TARGET DEFINITIONS", "",
				"TARGET", "For more configuration options add a dedicated section")
			;

		settings.alias().add_key_to_settings()
			("hostname", sh::string_key(&hostname_, "auto"),
				"HOSTNAME", "The host name of the monitored computer.\nSet this to auto (default) to use the windows name of the computer.\n\n"
				"auto\tHostname\n"
				"${host}\tHostname\n"
				"${host_lc}\nHostname in lowercase\n"
				"${host_uc}\tHostname in uppercase\n"
				"${domain}\tDomainname\n"
				"${domain_lc}\tDomainname in lowercase\n"
				"${domain_uc}\tDomainname in uppercase\n"
				)

			("channel", sh::string_key(&channel_, "NRDP"),
				"CHANNEL", "The channel to listen to.")

			;

		settings.register_all();
		settings.notify();

		client_.finalize(get_settings_proxy());

		nscapi::core_helper core(get_core(), get_id());
		core.register_channel(channel_);

		if (hostname_ == "auto") {
			hostname_ = boost::asio::ip::host_name();
		} else if (hostname_ == "auto-lc") {
			hostname_ = boost::asio::ip::host_name();
			std::transform(hostname_.begin(), hostname_.end(), hostname_.begin(), ::tolower);
		} else if (hostname_ == "auto-uc") {
			hostname_ = boost::asio::ip::host_name();
			std::transform(hostname_.begin(), hostname_.end(), hostname_.begin(), ::toupper);
		} else {
			str::utils::token dn = str::utils::getToken(boost::asio::ip::host_name(), '.');

			try {
				boost::asio::io_service svc;
				boost::asio::ip::tcp::resolver resolver(svc);
				boost::asio::ip::tcp::resolver::query query(boost::asio::ip::host_name(), "");
				boost::asio::ip::tcp::resolver::iterator iter = resolver.resolve(query), end;

				std::string s;
				while (iter != end) {
					s += iter->host_name();
					s += " - ";
					s += iter->endpoint().address().to_string();
					iter++;
				}
			} catch (const std::exception& e) {
				NSC_LOG_ERROR_EXR("Failed to resolve: ", e);
			}

			str::utils::replace(hostname_, "${host}", dn.first);
			str::utils::replace(hostname_, "${domain}", dn.second);
			std::transform(dn.first.begin(), dn.first.end(), dn.first.begin(), ::toupper);
			std::transform(dn.second.begin(), dn.second.end(), dn.second.begin(), ::toupper);
			str::utils::replace(hostname_, "${host_uc}", dn.first);
			str::utils::replace(hostname_, "${domain_uc}", dn.second);
			std::transform(dn.first.begin(), dn.first.end(), dn.first.begin(), ::tolower);
			std::transform(dn.second.begin(), dn.second.end(), dn.second.begin(), ::tolower);
			str::utils::replace(hostname_, "${host_lc}", dn.first);
			str::utils::replace(hostname_, "${domain_lc}", dn.second);
		}
		client_.set_sender(hostname_);
	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_EXR("NSClient API exception: ", e);
		return false;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_EXR("NSClient API exception: ", e);
		return false;
	} catch (...) {
		NSC_LOG_ERROR_EX("NSClient API exception: ");
		return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
// Settings helpers
//

void NRDPClient::add_target(std::string key, std::string arg) {
	try {
		client_.add_target(get_settings_proxy(), key, arg);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add target: " + key, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add target: " + key);
	}
}

void NRDPClient::add_command(std::string key, std::string arg) {
	try {
		nscapi::core_helper core(get_core(), get_id());
		std::string k = client_.add_command(key, arg);
		if (!k.empty())
			core.register_command(k.c_str(), "NRDP relay for: " + key);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add command: " + key, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add command: " + key);
	}
}

/**
 * Unload (terminate) module.
 * Attempt to stop the background processing thread.
 * @return true if successfully, false if not (if not things might be bad)
 */
bool NRDPClient::unloadModule() {
	client_.clear();
	return true;
}

void NRDPClient::query_fallback(const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message) {
	client_.do_query(request_message, response_message);
}

bool NRDPClient::commandLineExec(const int target_mode, const Plugin::ExecuteRequestMessage &request, Plugin::ExecuteResponseMessage &response) {
	if (target_mode == NSCAPI::target_module)
		return client_.do_exec(request, response, "submit_");
	return false;
}

void NRDPClient::handleNotification(const std::string &, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage *response_message) {
	client_.do_submit(request_message, *response_message);
}