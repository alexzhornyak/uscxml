/**
 *  @file
 *  @author     2012-2014 Stefan Radomski (stefan.radomski@cs.tu-darmstadt.de)
 *  @copyright  Simplified BSD
 *
 *  @cond
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the FreeBSD license as published by the FreeBSD
 *  project.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  You should have received a copy of the FreeBSD license along with this
 *  program. If not, see <http://www.opensource.org/licenses/bsd-license>.
 *  @endcond
 */

#include "uscxml/Common.h"
#include "uscxml/util/URL.h"
#include "uscxml/util/String.h"

#include "LuaDataModel.h"

// disable forcing to bool performance warning
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4800)
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "uscxml/messages/Event.h"
#ifndef NO_XERCESC
#include "uscxml/util/DOM.h"
#endif
#include "uscxml/interpreter/Logging.h"
#include <boost/algorithm/string.hpp>

//#include "LuaDOM.cpp.inc"

#ifdef BUILD_AS_PLUGINS
#include <Pluma/Connector.hpp>
#endif

namespace uscxml {

#ifdef BUILD_AS_PLUGINS
PLUMA_CONNECTOR
bool pluginConnect(pluma::Host& host) {
	host.add( new LuaDataModelProvider() );
	return true;
}
#endif

static int luaEval(lua_State* luaState, const std::string& expr) {
	int preStack = lua_gettop(luaState);
	int error = luaL_loadstring(luaState, expr.c_str()) || lua_pcall(luaState, 0, LUA_MULTRET, 0);
	if (error) {
		std::string errMsg = lua_tostring(luaState, -1);
		lua_pop(luaState, 1);  /* pop error message from the stack */
		ERROR_EXECUTION_THROW(errMsg);
	}
	int postStack = lua_gettop(luaState);
	return postStack - preStack;
}

Data LuaDataModel::getLuaAsData(lua_State* _luaState, const luabridge::LuaRef& lua) {
	Data data;
	if (lua.isFunction()) {
		// we are creating __tmpFunc
		// then it will be assigned to data variable
		luabridge::setGlobal(_luaState, lua, "__tmpFunc");
		data.atom = "__tmpFunc"; // safe in context of current interpreter, but!!! FORBIDDEN TO PASS IT TO ANOTHER INTERPRETER!!!
		data.type = Data::INTERPRETED;
	} else if(lua.isLightUserdata() || lua.isUserdata()) {
		luabridge::setGlobal(_luaState, lua, "__tmpUserData");
		data.atom = "__tmpUserData"; // DANGEROUS FOR USE, NOT RECOMMENDED
		data.type = Data::INTERPRETED;
	} else if(lua.isThread()) {
		luabridge::setGlobal(_luaState, lua, "__tmpThread");
		data.atom = "__tmpThread"; // DANGEROUS FOR USE, NOT RECOMMENDED
		data.type = Data::INTERPRETED;
	} else if(lua.isNil()) {
		data.atom = "nil";
		data.type = Data::INTERPRETED;
	} else if (lua.type() == LUA_TBOOLEAN) {
		data.atom = lua.cast<bool>() ? "true" : "false";
		data.type = Data::INTERPRETED;
	}
	else if(lua.isNumber()) {
		data.atom = toStr(lua.cast<double>());
		data.type = Data::INTERPRETED;
	} else if(lua.isString()) {
		data.atom = lua.cast<std::string>();
		data.type = Data::VERBATIM;
	} else if(lua.isTable()) {		
		for (luabridge::Iterator iter(lua); !iter.isNil(); ++iter) {
			luabridge::LuaRef luaKey = iter.key();
			luabridge::LuaRef luaVal = *iter;
			if (luaKey.isString()) {
				// luaKey.tostring() is not working?! see issue84
				data.compound[luaKey.cast<std::string>()] = getLuaAsData(_luaState, luaVal);
			}
			else {
				int i_key = luaKey.cast<double>();
				data.array.insert(std::make_pair(i_key,getLuaAsData(_luaState, luaVal)));
			}
		}
		// here may be the moment, when Lua table will be empty,
		// but we must prevent of returning empty data, 
		// that's why will make an interpreted atom="{}"
		if (data.array.empty() && data.compound.empty()) {
			data.atom = "{}";
			data.type = Data::INTERPRETED;
		}
	}	
	else {
		ERROR_EXECUTION_THROW("Lua type [" + std::to_string(lua.type()) + "] is not supported!");
	}
	return data;
}

luabridge::LuaRef LuaDataModel::getDataAsLua(lua_State* _luaState, const Data& data) {
	luabridge::LuaRef luaData (_luaState);

	if (data.node) {
		ERROR_EXECUTION_THROW("No DOM support in Lua datamodel");
	}
	
	// lua tables can be mixed!
	// tmp = { [1]=1,[2]=2,["test"]=5 }
	if (data.compound.size() > 0 || data.array.size() > 0) {
		luaData = luabridge::newTable(_luaState);

		for (auto it : data.array) {
			luaData[it.first] = getDataAsLua(_luaState, it.second);
		}
		std::map<std::string, Data>::const_iterator compoundIter = data.compound.begin();
		while(compoundIter != data.compound.end()) {
			luaData[compoundIter->first] = getDataAsLua(_luaState, compoundIter->second);
			compoundIter++;
		}		
		return luaData;
	}

	// there can be case when string is empty
	if (data.atom.size() > 0 || data.type == Data::VERBATIM) {
		switch (data.type) {
		case Data::VERBATIM: {
			luaData = data.atom;
			break;
		}
		case Data::INTERPRETED: {
			if (isNumeric(data.atom.c_str(), 10)) {
				// !!!! what about, when System Delimiter is not a DOT,
				// for example, Russian Keyaboard Layout ???
				if (data.atom.find(".") != std::string::npos) {
					luaData = strTo<double>(data.atom);
				}
				else {
					luaData = strTo<long>(data.atom);
				}
			}
			else {
				int retVals = luaEval(_luaState, "return(" + data.atom + ");");
				if (retVals == 1) {
					luaData = luabridge::LuaRef::fromStack(_luaState, -1);
				}
				lua_pop(_luaState, retVals);
			}
		}
		}
		return luaData;
	}
	// hopefully this is nil
	return luabridge::LuaRef(_luaState);
}

LuaDataModel::LuaDataModel() {
	_luaState = nullptr;
	_callbacks = nullptr;
}

int LuaDataModel::luaInFunction(lua_State * l) {
	luabridge::LuaRef ref = luabridge::getGlobal(l, "__datamodel");
	LuaDataModel* dm = ref.cast<LuaDataModel*>();

	int stackSize = lua_gettop(l);
	for (int i = 0; i < stackSize; i++) {
		if (!lua_isstring(l, -1 - i))
			continue;
		std::string stateName = lua_tostring(l, -1 - i);
		if (dm->_callbacks->isInState(stateName))
			continue;
		lua_pushboolean(l, 0);
		return 1;
	}
	lua_pushboolean(l, 1);
	return 1;
}

std::shared_ptr<DataModelImpl> LuaDataModel::create(DataModelCallbacks* callbacks) {
	std::shared_ptr<LuaDataModel> dm(new LuaDataModel());
	dm->_callbacks = callbacks;
	dm->setup();
	return dm;
}

void LuaDataModel::setup() {
	_luaState = luaL_newstate();
	luaL_openlibs(_luaState);

	luabridge::getGlobalNamespace(_luaState).beginClass<LuaDataModel>("DataModel").endClass();
	luabridge::setGlobal(_luaState, this, "__datamodel");

	// an option to create raw datamodel for custom needs
	if (_callbacks) {
		luabridge::getGlobalNamespace(_luaState).addCFunction("In", luaInFunction);

		luabridge::LuaRef ioProcTable = luabridge::newTable(_luaState);
		std::map<std::string, IOProcessor> ioProcs = _callbacks->getIOProcessors();
		std::map<std::string, IOProcessor>::const_iterator ioProcIter = ioProcs.begin();
		while (ioProcIter != ioProcs.end()) {
			Data ioProcData = ioProcIter->second.getDataModelVariables();
			ioProcTable[ioProcIter->first] = getDataAsLua(_luaState, ioProcData);
			ioProcIter++;
		}
		luabridge::setGlobal(_luaState, ioProcTable, "_ioprocessors");

		luabridge::LuaRef invTable = luabridge::newTable(_luaState);
		std::map<std::string, Invoker> invokers = _callbacks->getInvokers();
		std::map<std::string, Invoker>::const_iterator invIter = invokers.begin();
		while (invIter != invokers.end()) {
			Data invData = invIter->second.getDataModelVariables();
			invTable[invIter->first] = getDataAsLua(_luaState, invData);
			invIter++;
		}
		luabridge::setGlobal(_luaState, invTable, "_invokers");

		luabridge::setGlobal(_luaState, _callbacks->getName(), "_name");
		luabridge::setGlobal(_luaState, _callbacks->getSessionId(), "_sessionid");
	}
}

LuaDataModel::~LuaDataModel() {
	if (_luaState != NULL)
		lua_close(_luaState);
}

void LuaDataModel::addExtension(DataModelExtension* ext) {
	ERROR_EXECUTION_THROW("Extensions unimplemented in lua datamodel");
}

void LuaDataModel::setEvent(const Event& event) {
	luabridge::LuaRef luaEvent(_luaState);
	luaEvent = luabridge::newTable(_luaState);

	luaEvent["name"] = event.name;
	if (event.raw.size() > 0)
		luaEvent["raw"] = event.raw;
	if (event.origin.size() > 0)
		luaEvent["origin"] = event.origin;
	if (event.origintype.size() > 0)
		luaEvent["origintype"] = event.origintype;
	if (event.invokeid.size() > 0)
		luaEvent["invokeid"] = event.invokeid;
	if (!event.hideSendId)
		luaEvent["sendid"] = event.sendid;

	switch (event.eventType) {
	case Event::INTERNAL:
		luaEvent["type"] = "internal";
		break;
	case Event::EXTERNAL:
		luaEvent["type"] = "external";
		break;
	case Event::PLATFORM:
		luaEvent["type"] = "platform";
		break;

	default:
		break;
	}

	if (event.data.node) {
		ERROR_EXECUTION_THROW("No DOM support in Lua datamodel");		
	} else {
		// _event.data is KVP
		Data d = event.data;

		if (!event.params.empty()) {
			Event::params_t::const_iterator paramIter = event.params.begin();
			while(paramIter != event.params.end()) {
				d.compound[paramIter->first] = paramIter->second;
				paramIter++;
			}
		}
		if (!event.namelist.empty()) {
			Event::namelist_t::const_iterator nameListIter = event.namelist.begin();
			while(nameListIter != event.namelist.end()) {
				d.compound[nameListIter->first] = nameListIter->second;
				nameListIter++;
			}
		}

		if (!d.empty()) {
			luabridge::LuaRef luaData = getDataAsLua(_luaState, d);
			assert(luaEvent.isTable());
			// assert(luaData.isTable()); // not necessarily test179
			luaEvent["data"] = luaData;
		}
	}

	luabridge::setGlobal(_luaState, luaEvent, "_event");
}

Data LuaDataModel::evalAsData(const std::string& content) {
	Data data;

	std::string trimmedExpr = boost::trim_copy(content);

	int retVals = luaEval(_luaState, "return(" + trimmedExpr + ")");
	if (retVals == 1) {
		data = getLuaAsData(_luaState, luabridge::LuaRef::fromStack(_luaState, -1));
	}
	lua_pop(_luaState, retVals);
	return data;
}

void LuaDataModel::eval(const std::string& content) {
	std::string trimmedExpr = boost::trim_copy(content);

	int retVals = luaEval(_luaState, trimmedExpr);

	lua_pop(_luaState, retVals);
}

bool LuaDataModel::isValidExprSyntax(const std::string& expr) {
	
	const std::string s_expr = "return(" + expr + ")";
	
	int preStack = lua_gettop(_luaState);
	int err = luaL_loadstring (_luaState, s_expr.c_str());

	if (err) {
		std::string errMsg = lua_tostring(_luaState, -1);
		lua_pop(_luaState, 1);  /* pop error message from the stack */
		LOG(_callbacks->getLogger(), USCXML_ERROR) << errMsg << std::endl;
	}

	// clean stack again
	lua_pop(_luaState, lua_gettop(_luaState) - preStack);


	if (err == LUA_ERRSYNTAX)
		return false;

	return true;
}

bool LuaDataModel::isValidScriptSyntax(const std::string & expr)
{
	int preStack = lua_gettop(_luaState);
	int err = luaL_loadstring(_luaState, expr.c_str());

	if (err) {
		std::string errMsg = lua_tostring(_luaState, -1);
		lua_pop(_luaState, 1);  /* pop error message from the stack */
		LOG(_callbacks->getLogger(), USCXML_ERROR) << errMsg << std::endl;
	}

	// clean stack again
	lua_pop(_luaState, lua_gettop(_luaState) - preStack);

	if (err == LUA_ERRSYNTAX)
		return false;

	return true;
}

uint32_t LuaDataModel::getLength(const std::string& expr) {
	// we need the result of the expression on the lua stack -> has to "return"!
	std::string trimmedExpr = boost::trim_copy(expr);

	if (!boost::starts_with(trimmedExpr, "return")) {
		trimmedExpr = "return(#" + trimmedExpr + ")";
	}

	int retVals = luaEval(_luaState, trimmedExpr);

	if (retVals == 1 && lua_isnumber(_luaState, -1)) {
		int result = lua_tointeger(_luaState, -1);
		lua_pop(_luaState, 1);
		return result;
	}

	lua_pop(_luaState, retVals);
	ERROR_EXECUTION_THROW("'" + expr + "' does not evaluate to an array.");
	return 0;
}

void LuaDataModel::setForeach(const std::string& item,
                              const std::string& array,
                              const std::string& index,
                              uint32_t iteration) {
	iteration++; // test153: arrays start at 1

	const luabridge::LuaRef& arrRef = luabridge::getGlobal(_luaState, array.c_str());
	if (arrRef.isTable()) {

		// triggers syntax error for invalid items, test 152
		int retVals = luaEval(_luaState, item + " = " + array + "[" + toStr(iteration) + "]");
		lua_pop(_luaState, retVals);

		if (index.length() > 0) {
			int retVals = luaEval(_luaState, index + " = " + toStr(iteration));
			lua_pop(_luaState, retVals);
		}
	}
}

bool LuaDataModel::isDeclared(const std::string& expr) {
	// see: http://lua-users.org/wiki/DetectingUndefinedVariables
	return true;
}


void LuaDataModel::assign(const std::string& location, const Data& data, const std::map<std::string, std::string>& attr) {
	if (location.length() == 0) {
		ERROR_EXECUTION_THROW("Assign element has neither id nor location");
	}

	// flags on attribute are ignored?
	if (location.compare("_sessionid") == 0) // test 322
		ERROR_EXECUTION_THROW("Cannot assign to _sessionId");
	if (location.compare("_name") == 0)
		ERROR_EXECUTION_THROW("Cannot assign to _name");
	if (location.compare("_ioprocessors") == 0)  // test 326
		ERROR_EXECUTION_THROW("Cannot assign to _ioprocessors");
	if (location.compare("_invokers") == 0)
		ERROR_EXECUTION_THROW("Cannot assign to _invokers");
	if (location.compare("_event") == 0)
		ERROR_EXECUTION_THROW("Cannot assign to _event");

	if (data.node) {
		ERROR_EXECUTION_THROW("Cannot assign xml nodes in lua datamodel");
	} else {
		luabridge::LuaRef lua = getDataAsLua(_luaState, data);

		luabridge::setGlobal(_luaState, lua, "__tmpAssign");
		eval(location + "= __tmpAssign");
	}
}

void LuaDataModel::init(const std::string& location, const Data& data, const std::map<std::string, std::string>& attr) {
	luabridge::setGlobal(_luaState, luabridge::Nil(), location.c_str());
	assign(location, data);
}

bool LuaDataModel::evalAsBool(const std::string& expr) {
	// we need the result of the expression on the lua stack -> has to "return"!
	std::string trimmedExpr = boost::trim_copy(expr);

	int retVals = luaEval(_luaState, "return(" + trimmedExpr + ")");

	if (retVals == 1) {
		bool result = lua_toboolean(_luaState, -1);
		lua_pop(_luaState, 1);
		return result;
	}
	lua_pop(_luaState, retVals);

	return false;
}

Data LuaDataModel::getAsData(const std::string& content) {
	Data data;
	std::string trimmedExpr = boost::trim_copy(content);

	int retVals = luaEval(_luaState, "__tmp = " + content + "; return __tmp");
	if (retVals == 1) {
		data = getLuaAsData(_luaState, luabridge::LuaRef::fromStack(_luaState, -1));
	}
	lua_pop(_luaState, retVals);

	// escape as a string, this is sometimes the case with <content>
	if (data.atom == "nil" && data.type == Data::INTERPRETED) {
		int retVals = luaEval(_luaState, "__tmp = '" + content + "'; return __tmp");
		if (retVals == 1) {
			data = getLuaAsData(_luaState, luabridge::LuaRef::fromStack(_luaState, -1));
		}
		lua_pop(_luaState, retVals);
	}

	return data;
}

}
