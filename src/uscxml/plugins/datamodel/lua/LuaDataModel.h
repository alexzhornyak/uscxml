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

#ifndef LUADATAMODEL_H_113E014C
#define LUADATAMODEL_H_113E014C

#include "uscxml/config.h"
#include "uscxml/plugins/DataModelImpl.h"
#include <list>

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include "LuaBridge.h"

#ifdef BUILD_AS_PLUGINS
#include "uscxml/plugins/Plugins.h"
#endif


namespace uscxml {
class Event;
class Data;
}

namespace uscxml {

/**
 * @ingroup datamodel
 * Lua data-model.
 */

class USCXML_API LuaDataModel : public DataModelImpl {
public:
	LuaDataModel();
	virtual ~LuaDataModel();
	virtual std::shared_ptr<DataModelImpl> create(DataModelCallbacks* callbacks) override;

	virtual void addExtension(DataModelExtension* ext) override;

	virtual std::list<std::string> getNames() override {
		std::list<std::string> names;
		names.push_back("lua");
		return names;
	}

	virtual bool isValidSyntax(const std::string& expr) override;

	virtual bool isValidScriptSyntax(const std::string& expr) override;

	virtual void setEvent(const Event& event) override;

	// foreach
	virtual uint32_t getLength(const std::string& expr) override;
	virtual void setForeach(const std::string& item,
	                        const std::string& array,
	                        const std::string& index,
	                        uint32_t iteration) override;

	virtual bool evalAsBool(const std::string& expr) override;
	virtual Data evalAsData(const std::string& expr) override;
	virtual void eval(const std::string& content) override;
	virtual Data getAsData(const std::string& content) override;

	virtual bool isDeclared(const std::string& expr) override;

	virtual void assign(const std::string& location,
	                    const Data& data,
	                    const std::map<std::string, std::string>& attr = std::map<std::string, std::string>()) override;
	virtual void init(const std::string& location,
	                  const Data& data,
	                  const std::map<std::string, std::string>& attr = std::map<std::string, std::string>()) override;

	static Data getLuaAsData(lua_State* _luaState, const luabridge::LuaRef& lua);

	static luabridge::LuaRef getDataAsLua(lua_State* _luaState, const Data& data);

protected:
	virtual void setup() override;

	static int luaInFunction(lua_State * l);

	lua_State* _luaState;
};

#ifdef BUILD_AS_PLUGINS
PLUMA_INHERIT_PROVIDER(LuaDataModel, DataModelImpl)
#endif

}

#endif /* end of include guard: LUADATAMODEL_H_113E014C */
