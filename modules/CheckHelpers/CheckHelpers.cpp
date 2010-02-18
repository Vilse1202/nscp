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
#include "CheckHelpers.h"
#include <strEx.h>
#include <time.h>
#include <utils.h>

CheckHelpers gCheckHelpers;

CheckHelpers::CheckHelpers() {
}
CheckHelpers::~CheckHelpers() {
}


bool CheckHelpers::loadModule(NSCAPI::moduleLoadMode mode) {
	try {
		GET_CORE()->registerCommand(_T("CheckAlwaysOK"), _T("Run another check and regardless of its return code return OK."));
		GET_CORE()->registerCommand(_T("CheckAlwaysCRITICAL"), _T("Run another check and regardless of its return code return CRIT."));
		GET_CORE()->registerCommand(_T("CheckAlwaysWARNING"), _T("Run another check and regardless of its return code return WARN."));
		GET_CORE()->registerCommand(_T("CheckMultiple"), _T("Run more then one check and return the worst state."));
		GET_CORE()->registerCommand(_T("CheckOK"), _T("Just return OK (anything passed along will be used as a message)."));
		GET_CORE()->registerCommand(_T("check_ok"), _T("Just return OK (anything passed along will be used as a message)."));
		GET_CORE()->registerCommand(_T("CheckWARNING"), _T("Just return WARN (anything passed along will be used as a message)."));
		GET_CORE()->registerCommand(_T("CheckCRITICAL"), _T("Just return CRIT (anything passed along will be used as a message)."));
		GET_CORE()->registerCommand(_T("CheckVersion"), _T("Just return the nagios version (along with OK status)."));
	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + e.msg_);
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command."));
	}
	return true;
}
bool CheckHelpers::unloadModule() {
	return true;
}

bool CheckHelpers::hasCommandHandler() {
	return true;
}
bool CheckHelpers::hasMessageHandler() {
	return false;
}
NSCAPI::nagiosReturn CheckHelpers::checkSimpleStatus(NSCAPI::nagiosReturn status, const std::list<std::wstring> arguments, std::wstring &message, std::wstring &perf) 
{
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	if (arguments.empty()) {
		message = nscapi::plugin_helper::translateReturn(status) + _T(": Lets pretend everything is going to be ok.");
		return status;
	}
	std::list<std::wstring>::const_iterator cit;
	for (cit=arguments.begin();cit!=arguments.end();++cit)
		message += *cit;
	return status;
}

NSCAPI::nagiosReturn CheckHelpers::handleCommand(const std::wstring command, std::list<std::wstring> arguments, std::wstring &message, std::wstring &perf) {
	if (command == _T("CheckVersion")) {
		message = GET_CORE()->getApplicationVersionString();
		return NSCAPI::returnOK;
	} else if (command == _T("CheckAlwaysOK")) {
		if (arguments.size() < 1) {
			message = _T("ERROR: Missing arguments.");
			return NSCAPI::returnUNKNOWN;
		}
		std::wstring new_command = arguments.front(); arguments.pop_front();
		GET_CORE()->InjectSimpleCommand(new_command, arguments, message, perf);
		return NSCAPI::returnOK;
	} else if (command == _T("CheckAlwaysCRITICAL")) {
		if (arguments.size() < 1) {
			message = _T("ERROR: Missing arguments.");
			return NSCAPI::returnUNKNOWN;
		}
		std::wstring new_command = arguments.front(); arguments.pop_front();
		GET_CORE()->InjectSimpleCommand(new_command, arguments, message, perf);
		return NSCAPI::returnCRIT;
	} else if (command == _T("CheckAlwaysWARNING")) {
		if (arguments.size() < 1) {
			message = _T("ERROR: Missing arguments.");
			return NSCAPI::returnUNKNOWN;
		}
		std::wstring new_command = arguments.front(); arguments.pop_front();
		GET_CORE()->InjectSimpleCommand(new_command, arguments, message, perf);
		return NSCAPI::returnWARN;
	} else if (command == _T("CheckOK")) {
		return checkSimpleStatus(NSCAPI::returnOK, arguments, message, perf);
	} else if (command == _T("check_ok")) {
		return checkSimpleStatus(NSCAPI::returnOK, arguments, message, perf);
	} else if (command == _T("CheckWARNING")) {
		return checkSimpleStatus(NSCAPI::returnWARN, arguments, message, perf);
	} else if (command == _T("CheckCRITICAL")) {
		return checkSimpleStatus(NSCAPI::returnCRIT, arguments, message, perf);
	} else if (command == _T("CheckMultiple")) {
		return checkMultiple(arguments, message, perf);
	}
	return NSCAPI::returnIgnored;
}
NSCAPI::nagiosReturn CheckHelpers::checkMultiple(const std::list<std::wstring> arguments, std::wstring &message, std::wstring &perf) 
{
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	if (arguments.empty()) {
		message = _T("Missing argument(s).");
		return NSCAPI::returnCRIT;
	}
	typedef std::pair<std::wstring, std::list<std::wstring> > sub_command;
	std::list<sub_command> commands;
	sub_command currentCommand;
	std::list<std::wstring>::const_iterator cit;
	for (cit=arguments.begin();cit!=arguments.end();++cit) {
		std::wstring arg = *cit;
		std::pair<std::wstring,std::wstring> p = strEx::split(arg,_T("="));
		if (p.first == _T("command")) {
			if (!currentCommand.first.empty())
				commands.push_back(currentCommand);
			currentCommand.first = p.second;
			currentCommand.second.clear();
		} else {
			currentCommand.second.push_back(*cit);
		}
	}
	if (!currentCommand.first.empty())
		commands.push_back(currentCommand);
	std::list<sub_command>::iterator cit2;
	for (cit2 = commands.begin(); cit2 != commands.end(); ++cit2) {
		std::list<std::wstring> sub_args;
		std::wstring tMsg, tPerf;
		NSCAPI::nagiosReturn tRet = GET_CORE()->InjectSimpleCommand((*cit2).first.c_str(), (*cit2).second, tMsg, tPerf);
		returnCode = nscapi::plugin_helper::maxState(returnCode, tRet);
		if (!message.empty())
			message += _T(", ");
		message += tMsg;
		perf += tPerf;
	}
	return returnCode;
}


NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(gCheckHelpers);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gCheckHelpers);
