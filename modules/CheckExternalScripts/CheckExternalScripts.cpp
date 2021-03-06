/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#include "stdafx.h"
#include "CheckExternalScripts.h"
#include <time.h>
#include <string>

#include <strEx.h>
#include <file_helpers.hpp>

#include <boost/regex.hpp>
#include <boost/filesystem.hpp>

#include <settings/client/settings_client.hpp>
#include <nscapi/functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>

#include <config.h>

namespace sh = nscapi::settings_helper;

CheckExternalScripts::CheckExternalScripts() {}
CheckExternalScripts::~CheckExternalScripts() {}

void CheckExternalScripts::addAllScriptsFrom(std::wstring str_path) {
	boost::filesystem::path path = utf8::cvt<std::string>(str_path);
	if (path.has_relative_path())
		path = get_base_path() / path;
	file_helpers::patterns::pattern_type split_path = file_helpers::patterns::split_pattern(path);
	if (!boost::filesystem::is_directory(split_path.first))
		NSC_LOG_ERROR_STD("Path was not found: " + split_path.first.string());

	boost::regex pattern(split_path.second.string());
	boost::filesystem::directory_iterator end_itr; // default construction yields past-the-end
	for ( boost::filesystem::directory_iterator itr( split_path.first ); itr != end_itr; ++itr ) {
		if ( !is_directory(itr->status()) ) {
#ifdef WIN32
			std::string name = itr->path().leaf().string();
#else
			std::string name = itr->path().leaf();
#endif
			if (regex_match(name, pattern))
				add_command(name, (split_path.first / name).string());
		}
	}
}


bool CheckExternalScripts::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
	try {

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(alias, "external scripts");

		commands_path = settings.alias().get_settings_path("scripts");
		aliases_path = settings.alias().get_settings_path("alias");
		std::string wrappings_path = settings.alias().get_settings_path("wrappings");

		settings.alias().add_path_to_settings()

			("wrappings", sh::string_map_path(&wrappings_)
			, "EXTERNAL SCRIPT WRAPPINGS SECTION", "A list of templates for wrapped scripts")

			("alias", sh::fun_values_path(boost::bind(&CheckExternalScripts::add_alias, this, _1, _2)), 
			"EXTERNAL SCRIPT ALIAS SECTION", "A list of aliases available. An alias is an internal command that has been \"wrapped\" (to add arguments). Be careful so you don't create loops (ie check_loop=check_a, check_a=check_loop)")

			;

		settings.register_all();
		settings.notify();
		settings.clear();

		if (wrappings_.empty()) {
			NSC_DEBUG_MSG("No wrappings found (adding default: vbs, ps1 and bat)");
			wrappings_["vbs"] = "cscript.exe //T:30 //NoLogo scripts\\\\lib\\\\wrapper.vbs %SCRIPT% %ARGS%";
			wrappings_["ps1"] = "cmd /c echo scripts\\\\%SCRIPT% %ARGS%; exit($lastexitcode) | powershell.exe -command -";
			wrappings_["bat"] = "scripts\\\\%SCRIPT% %ARGS%";
			settings.register_key(wrappings_path, "vbs", NSCAPI::key_string, "VISUAL BASIC WRAPPING", "", wrappings_["vbs"], false);
			settings.register_key(wrappings_path, "ps1", NSCAPI::key_string, "POWERSHELL WRAPPING", "", wrappings_["ps1"], false);
			settings.register_key(wrappings_path, "bat", NSCAPI::key_string, "BATCH FILE WRAPPING", "", wrappings_["bat"], false);
		}

		if (aliases_.empty()) {
			NSC_DEBUG_MSG("No aliases found (adding default)");

			add_alias("alias_cpu", "checkCPU warn=80 crit=90 time=5m time=1m time=30s");
			add_alias("alias_cpu_ex", "checkCPU warn=$ARG1$ crit=$ARG2$ time=5m time=1m time=30s");
			add_alias("alias_mem", "checkMem MaxWarn=80% MaxCrit=90% ShowAll=long type=physical type=virtual type=paged type=page");
			add_alias("alias_up", "checkUpTime MinWarn=1d MinWarn=1h");
			add_alias("alias_disk", "CheckDriveSize MinWarn=10% MinCrit=5% CheckAll FilterType=FIXED");
			add_alias("alias_disk_loose", "CheckDriveSize MinWarn=10% MinCrit=5% CheckAll FilterType=FIXED ignore-unreadable");
			add_alias("alias_volumes", "CheckDriveSize MinWarn=10% MinCrit=5% CheckAll=volumes FilterType=FIXED");
			add_alias("alias_volumes_loose", "CheckDriveSize MinWarn=10% MinCrit=5% CheckAll=volumes FilterType=FIXED ignore-unreadable ");
			add_alias("alias_service", "checkServiceState CheckAll");
			add_alias("alias_service_ex", "checkServiceState CheckAll \"exclude=Net Driver HPZ12\" \"exclude=Pml Driver HPZ12\" exclude=stisvc");
			add_alias("alias_process", "checkProcState \"$ARG1$=started\"");
			add_alias("alias_process_stopped", "checkProcState \"$ARG1$=stopped\"");
			add_alias("alias_process_count", "checkProcState MaxWarnCount=$ARG2$ MaxCritCount=$ARG3$ \"$ARG1$=started\"");
			add_alias("alias_process_hung", "checkProcState MaxWarnCount=1 MaxCritCount=1 \"$ARG1$=hung\"");
			add_alias("alias_event_log", "CheckEventLog file=application file=system MaxWarn=1 MaxCrit=1 \"filter=generated gt -2d AND severity NOT IN ('success', 'informational') AND source != 'SideBySide'\" truncate=800 unique descriptions \"syntax=%severity%: %source%: %message% (%count%)\"");
			add_alias("alias_file_size", "CheckFiles \"filter=size > $ARG2$\" \"path=$ARG1$\" MaxWarn=1 MaxCrit=1 \"syntax=%filename% %size%\" max-dir-depth=10");
			add_alias("alias_file_age", "checkFile2 filter=out \"file=$ARG1$\" filter-written=>1d MaxWarn=1 MaxCrit=1 \"syntax=%filename% %write%\"");
			add_alias("alias_sched_all", "CheckTaskSched \"filter=exit_code ne 0\" \"syntax=%title%: %exit_code%\" warn=>0");
			add_alias("alias_sched_long", "CheckTaskSched \"filter=status = 'running' AND most_recent_run_time < -$ARG1$\" \"syntax=%title% (%most_recent_run_time%)\" warn=>0");
			add_alias("alias_sched_task", "CheckTaskSched \"filter=title eq '$ARG1$' AND exit_code ne 0\" \"syntax=%title% (%most_recent_run_time%)\" warn=>0");
			add_alias("alias_updates", "check_updates -warning 0 -critical 0");
		}

		settings.alias().add_path_to_settings()
			("EXTERNAL SCRIPT SECTION", "Section for external scripts configuration options (CheckExternalScripts).")

			("scripts", sh::fun_values_path(boost::bind(&CheckExternalScripts::add_command, this, _1, _2)),
			"EXTERNAL SCRIPT SCRIPT SECTION", "A list of scripts available to run from the CheckExternalScripts module. Syntax is: <command>=<script> <arguments>")

			("wrapped scripts", sh::fun_values_path(boost::bind(&CheckExternalScripts::add_wrapping, this, _1, _2)), 
			"EXTERNAL SCRIPT WRAPPED SCRIPTS SECTION", "A list of wrappped scripts (ie. using the template mechanism)")

			;

		settings.alias().add_key_to_settings()
			("timeout", sh::uint_key(&timeout, 60),
			"COMMAND TIMEOUT", "The maximum time in seconds that a command can execute. (if more then this execution will be aborted). NOTICE this only affects external commands not internal ones.")

			("allow arguments", sh::bool_key(&allowArgs_, false),
			"COMMAND ARGUMENT PROCESSING", "This option determines whether or not the we will allow clients to specify arguments to commands that are executed.")

			("allow nasty characters", sh::bool_key(&allowNasty_, false),
			"COMMAND ALLOW NASTY META CHARS", "This option determines whether or not the we will allow clients to specify nasty (as in |`&><'\"\\[]{}) characters in arguments.")

			("script path", sh::wstring_key(&scriptDirectory_),
			"SCRIPT DIRECTORY", "Load all scripts in a directory and use them as commands. Probably dangerous but useful if you have loads of scripts :)")
			;

		settings.register_all();
		settings.notify();


		if (!scriptDirectory_.empty()) {
			addAllScriptsFrom(scriptDirectory_);
		}
		root_ = get_base_path();

		nscapi::core_helper::core_proxy core(get_core(), get_id());
		BOOST_FOREACH(const commands::command_handler::object_list_type::value_type &o, commands_.object_list) {
			core.register_command(o.second.alias, "Alias for: " + o.second.alias);
		}
		BOOST_FOREACH(const commands::command_handler::object_list_type::value_type &o, aliases_.object_list) {
			core.register_command(o.second.alias, "Alias for: " + o.second.alias);
		}


// 	} catch (nrpe::server::nrpe_exception &e) {
// 		NSC_LOG_ERROR_STD(_T("Exception caught: ") + e.what());
// 		return false;
	} catch (...) {
		NSC_LOG_ERROR_EX("loading");
		return false;
	}
	return true;
}
bool CheckExternalScripts::unloadModule() {
	return true;
}

void CheckExternalScripts::add_command(std::string key, std::string arg) {
	try {
		commands_.add(get_settings_proxy(), commands_path, key, arg, key == "default");
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add: " + key, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add: " + key);
	}
}
void CheckExternalScripts::add_alias(std::string key, std::string arg) {
	try {
		aliases_.add(get_settings_proxy(), aliases_path, key, arg, key == "default");
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add: " + key, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add: " + key);
	}
}


void CheckExternalScripts::query_fallback(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &request_message) {
	commands::optional_command_object cmd = commands_.find_object(request.command());
	bool isAlias = !cmd;
	if (!cmd)
		cmd = aliases_.find_object(request.command());
	if (!cmd)
		return nscapi::protobuf::functions::set_response_bad(*response, "Command not found: " + request.command());

	const commands::command_object cd = *cmd;
	std::list<std::string> args = cd.arguments;
	bool first = true;
	if (isAlias || allowArgs_) {
		BOOST_FOREACH(std::string &arg, args) {
			for(int i=0;i<request.arguments_size();i++) {
				const std::string &replace_arg = request.arguments(i);
				if (first && !isAlias && !allowNasty_) {
					if (replace_arg.find_first_of(NASTY_METACHARS) != std::wstring::npos) {
						return nscapi::protobuf::functions::set_response_bad(*response, "Request contained illegal characters!");
					}
				}
				strEx::replace(arg, "$ARG" + strEx::s::xtos(i+1) + "$", replace_arg);
			}
		}
	} else if (!allowArgs_ && request.arguments_size() > 0) {
		return nscapi::protobuf::functions::set_response_bad(*response, "Arguments not allowed in CheckExternalScripts set /settings/external scripts/allow arguments=true");
	}


	std::string xargs;
	BOOST_FOREACH(std::string s, args) {
		if (!xargs.empty())
			xargs += " ";
		xargs += + "\"" + s + "\"";
	}

	if (isAlias) {
		std::wstring message;
		try {
			std::string buffer;
			int result = nscapi::core_helper::simple_query(cd.command, args, buffer);
			if (result == NSCAPI::returnIgnored)
				return nscapi::protobuf::functions::set_response_bad(*response, "No handler for command: " + cd.command);
			Plugin::QueryResponseMessage local_response;
			local_response.ParseFromString(buffer);
			if (local_response.payload_size() != 1)
				return nscapi::protobuf::functions::set_response_bad(*response, "Invalid return from: " + cd.command);
			response->CopyFrom(local_response.payload(0));
		} catch (boost::escaped_list_error &e) {
			NSC_LOG_MESSAGE_STD("Failed to parse alias expression: " + e.what());
			NSC_LOG_MESSAGE("We will now try parsing the old syntax instead...");
			std::string buffer;
			if (nscapi::core_helper::simple_query(cd.command, args, buffer) != NSCAPI::isSuccess) {
				return nscapi::protobuf::functions::set_response_bad(*response, "Failed to execute process: " + cd.command);
			}
			Plugin::QueryResponseMessage local_result;
			local_result.ParseFromString(buffer);
			if (local_result.payload_size() != 1) 
				return nscapi::protobuf::functions::set_response_bad(*response, "Failed to execute process (invalid response): " + cd.command);
			response->CopyFrom(local_result.payload(0));
		}
	} else {
		std::string message, perf;
		process::exec_arguments args(root_, cd.command + " " + xargs, timeout);
		if (!cd.user.empty()) {
			args.user = cd.user;
			args.domain = cd.domain;
			args.password = cd.password;
		}
		int result = process::executeProcess(args, message, perf);
		if (!nscapi::plugin_helper::isNagiosReturnCode(result))
			return nscapi::protobuf::functions::set_response_bad(*response, "The command (" + args.command + ") returned an invalid return code: " + strEx::s::xtos(result));
		response->set_result(nscapi::protobuf::functions::nagios_status_to_gpb(result));
		response->set_message(message);
		nscapi::protobuf::functions::parse_performance_data(response, perf);
	}
}
