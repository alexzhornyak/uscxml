/**
 *  @file
 *  @author     2012-2013 Stefan Radomski (stefan.radomski@cs.tu-darmstadt.de)
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

#ifndef FACTORY_H_5WKLGPRB
#define FACTORY_H_5WKLGPRB

#include "uscxml/Common.h"

#include <string.h>

#ifdef BUILD_AS_PLUGINS
#include "Pluma/Pluma.hpp"
#endif

#include <memory>
#include <set>
#include <map>
#include <unordered_map>
#include <string>
#include <limits>

namespace uscxml {

class InterpreterImpl;
class IOProcessorImpl;
class IOProcessorCallbacks;
class DataModelImpl;
class DataModelCallbacks;
class InvokerImpl;
class InvokerCallbacks;
class ExecutableContentImpl;

class USCXML_API Factory {
public:
	Factory(Factory* parentFactory);
	Factory(const std::string& pluginPath, Factory* parentFactory);

	/* 
		Previously 'registerPlugins' method was in constructor and all ioprocessors, datamodels etc were invoked
		Sometimes we do not want to create all of them. That's why we let user to decide, when to initialize them.
		You can call either 'registerPlugins' to register all of available or to call 'register' separately
	*/
	void registerPlugins();

	enum PluginType {
		ptSCXMLIOProcessor,
		ptBasicHTTPIOProcessor,
		ptV8DataModel,
		ptJSCDataModel,
		ptLuaDataModel,
		ptC89DataModel,
		ptPromelaDataModel,
		ptNullDataModel,
		ptUSCXMLInvoker,
		ptDirMonInvoker,
		ptALL
	};

	void registerCustomPlugins(const std::set<PluginType> &pluginTypes);

	void registerIOProcessor(std::shared_ptr<IOProcessorImpl> ioProcessor);
	void registerDataModel(std::shared_ptr<DataModelImpl> dataModel);
	void registerInvoker(std::shared_ptr<InvokerImpl> invoker);
	void registerExecutableContent(std::shared_ptr<ExecutableContentImpl> executableContent);

	std::shared_ptr<DataModelImpl> createDataModel(const std::string& type, DataModelCallbacks* callbacks);
	std::shared_ptr<IOProcessorImpl> createIOProcessor(const std::string& type, IOProcessorCallbacks* callbacks);
	std::shared_ptr<InvokerImpl> createInvoker(const std::string& type, InvokerCallbacks* interpreter);
	std::shared_ptr<ExecutableContentImpl> createExecutableContent(const std::string& localName, const std::string& nameSpace, InterpreterImpl* interpreter);

	bool hasDataModel(const std::string& type);
	bool hasIOProcessor(const std::string& type);
	bool hasInvoker(const std::string& type);
	bool hasExecutableContent(const std::string& localName, const std::string& nameSpace);

	std::map<std::string, IOProcessorImpl*> getIOProcessors();

	void listComponents();

	static Factory& getInstance();

	static void cleanup();

	static void setDefaultPluginPath(const std::string& path);
	static std::string getDefaultPluginPath();

protected:
	std::unordered_map<std::string, std::shared_ptr<DataModelImpl>> _dataModels;
	std::unordered_map<std::string, std::string> _dataModelAliases;
	std::unordered_map<std::string, std::shared_ptr<IOProcessorImpl>> _ioProcessors;
	std::unordered_map<std::string, std::string> _ioProcessorAliases;
	std::unordered_map<std::string, std::shared_ptr<InvokerImpl>> _invokers;
	std::unordered_map<std::string, std::string> _invokerAliases;
	std::map<std::pair<std::string, std::string>, std::shared_ptr<ExecutableContentImpl>> _executableContent;


#ifdef BUILD_AS_PLUGINS
	pluma::Pluma pluma;
#endif
	
	Factory(const std::string&);
	~Factory();
	Factory* _parentFactory = NULL;
	std::string _pluginPath;
	static std::string _defaultPluginPath;
	static Factory* _instance;

};


}

#endif /* end of include guard: FACTORY_H_5WKLGPRB */
