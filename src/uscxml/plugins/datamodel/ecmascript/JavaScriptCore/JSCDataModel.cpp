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

#include "uscxml/Common.h"
#include "uscxml/util/URL.h"
#include "uscxml/util/String.h"

#include "JSCDataModel.h"

#include "uscxml/messages/Event.h"
#ifndef NO_XERCESC
#include "uscxml/util/DOM.h"
#endif
#include "uscxml/interpreter/Logging.h"

#include <vector>
#include <string>
#include <boost/algorithm/string.hpp>

#ifdef BUILD_AS_PLUGINS
#include <Pluma/Connector.hpp>
#endif

#define EVENT_STRING_OR_UNDEF(field, cond) \
JSStringRef field##Name = JSStringCreateWithUTF8CString( uscxml::fromLocaleToUtf8(#field).c_str() ); \
JSStringRef field##Val = JSStringCreateWithUTF8CString( uscxml::fromLocaleToUtf8(event.field).c_str()); \
JSObjectSetProperty(_ctx, \
                    eventObj, \
                    field##Name, \
                    (cond ? JSValueMakeString(_ctx, field##Val) : JSValueMakeUndefined(_ctx)), \
                    0, \
                    &exception); \
JSStringRelease(field##Name); \
JSStringRelease(field##Val); \
if (exception) \
    handleException(exception, std::string("event." #field ":") + "[" + event.field + "]");

using namespace XERCESC_NS;

static std::string JS2String(JSStringRef strRef) {
	if (strRef) {
		const size_t maxSize = JSStringGetMaximumUTF8CStringSize(strRef);
		std::vector<char> buffer(maxSize, 0);
		JSStringGetUTF8CString(strRef, buffer.data(), maxSize);
		return uscxml::toLocaleFromUtf8(buffer.data());
	}
	return "";
}

static std::string JS2StringAndRelease(JSStringRef strRef) {
	if (strRef) {
		const size_t maxSize = JSStringGetMaximumUTF8CStringSize(strRef);
		std::vector<char> buffer(maxSize, 0);
		JSStringGetUTF8CString(strRef, buffer.data(), maxSize);
		JSStringRelease(strRef);
		return uscxml::toLocaleFromUtf8(buffer.data());
	}
	return "";
}

#ifndef NO_XERCESC

static JSValueRef XMLString2JS(const XMLCh* input, JSContextRef context) {
	JSValueRef output;

	char* res = XERCESC_NS::XMLString::transcode(input);

	JSStringRef stringRef = JSStringCreateWithUTF8CString(uscxml::fromLocaleToUtf8(res).c_str());
	output = JSValueMakeString(context, stringRef);
	JSStringRelease(stringRef);

	return output;
}

static XMLCh* JS2XMLString(JSValueRef input, JSContextRef context) {

	if (!JSValueIsString(context, input))
		return NULL;

	JSValueRef exception = NULL;
	JSStringRef stringInput = JSValueToStringCopy(context, input, &exception);

	XMLCh* ret = XERCESC_NS::XMLString::transcode(JS2String(stringInput).c_str());
	return(ret);
}

#include "JSCDOM.cpp.inc"
#else
#include "JSCEvent.cpp.inc"
#endif

namespace uscxml {
	
	static void DumpJSObject(uscxml::Logger &logger, JSContextRef ctx, JSObjectRef objRef) {
		if (objRef) {
			JSPropertyNameArrayRef properties = JSObjectCopyPropertyNames(ctx, objRef);
			size_t paramCount = JSPropertyNameArrayGetCount(properties);
			for (size_t i = 0; i < paramCount; i++) {
				logger.log(uscxml::USCXML_LOG) << JS2StringAndRelease(JSPropertyNameArrayGetNameAtIndex(properties, i)) << std::endl;
			}
		}
	}

	class JSComplexClassException : public std::runtime_error {
	public:
		JSComplexClassException() : std::runtime_error("Complex JavaScript class") {}
	};

#ifdef BUILD_AS_PLUGINS
	PLUMA_CONNECTOR
		bool pluginConnect(pluma::Host& host) {
		host.add(new JSCDataModelProvider());
		return true;
	}
#endif

	JSCDataModel::JSCDataModel() {
		_ctx = NULL;
	}

	JSCDataModel::~JSCDataModel() {
		if (_ctx)
			JSGlobalContextRelease(_ctx);
	}

	void JSCDataModel::addExtension(DataModelExtension* ext) {
#if 0
		if (_extensions.find(ext) != _extensions.end())
			return;

		ext->dm = this;
		_extensions.insert(ext);

		JSObjectRef currScope = JSContextGetGlobalObject(_ctx);
		std::list<std::string> locPath = tokenize(ext->provides(), '.');
		std::list<std::string>::iterator locIter = locPath.begin();
		while (true) {
			std::string pathComp = *locIter;
			JSStringRef pathCompJS = JSStringCreateWithUTF8CString(pathComp.c_str());

			if (++locIter != locPath.end()) {
				// just another intermediate step
				if (!JSObjectHasProperty(_ctx, currScope, pathCompJS)) {
					JSObjectSetProperty(_ctx, currScope, pathCompJS, JSObjectMake(_ctx, NULL, NULL), kJSPropertyAttributeNone, NULL);
				}
				JSValueRef newScope = JSObjectGetProperty(_ctx, currScope, pathCompJS, NULL);
				JSStringRelease(pathCompJS);


				if (JSValueIsObject(_ctx, newScope)) {
					currScope = JSValueToObject(_ctx, newScope, NULL);
				}
				else {
					JSStringRelease(pathCompJS);
					throw "Cannot add datamodel extension in non-object";
				}
			}
			else {
				// this is the function!
				JSClassRef jsExtensionClassRef = JSClassCreate(&jsExtensionClassDef);
				JSObjectRef jsExtFuncObj = JSObjectMake(_ctx, jsExtensionClassRef, ext);
				JSObjectSetProperty(_ctx, currScope, pathCompJS, jsExtFuncObj, kJSPropertyAttributeNone, NULL);

				JSStringRelease(pathCompJS);
				break;
			}
		}
#endif
	}

	JSValueRef JSCDataModel::jsExtension(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
		DataModelExtension* extension = (DataModelExtension*)JSObjectGetPrivate(function);

		JSStringRef memberRef;
		std::string memberName = "";

		if (argumentCount > 0 && JSValueIsString(ctx, arguments[0])) {
			memberRef = JSValueToStringCopy(ctx, arguments[0], exception);
			memberName = JS2StringAndRelease(memberRef);
		}

		if (argumentCount > 1) {
			Data data;
			try {
				data = ((JSCDataModel*)(extension->dm))->getValueAsData(arguments[1]);
			}
			catch (JSComplexClassException &ex) {				
				ERROR_PLATFORM_THROW(ex.what());
			}
			// setter			
			extension->setValueOf(memberName, data);
			return JSValueMakeNull(ctx);
		}
		if (argumentCount == 1) {
			// getter
			return ((JSCDataModel*)(extension->dm))->getDataAsValue(extension->getValueOf(memberName));
		}

		return JSValueMakeNull(ctx);
	}

	// functions need to be objects to hold private data in JSC
	JSClassDefinition JSCDataModel::jsInClassDef = { 0, 0, "In", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, jsIn, 0, 0, 0 };
	JSClassDefinition JSCDataModel::jsPrintClassDef = { 0, 0, "print", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, jsPrint, 0, 0, 0 };
	JSClassDefinition JSCDataModel::jsExtensionClassDef = { 0, 0, "Extension", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, jsExtension, 0, 0, 0 };

	JSClassDefinition JSCDataModel::jsIOProcessorsClassDef = { 0, 0, "ioProcessors", 0, 0, 0, 0, 0, jsIOProcessorHasProp, jsIOProcessorGetProp, 0, 0, jsIOProcessorListProps, 0, 0, 0, 0 };
	JSClassDefinition JSCDataModel::jsInvokersClassDef = { 0, 0, "invokers", 0, 0, 0, 0, 0, jsInvokerHasProp, jsInvokerGetProp, 0, 0, jsInvokerListProps, 0, 0, 0, 0 };

	std::mutex JSCDataModel::_initMutex;

#ifndef NO_XERCESC

	bool JSCNodeListHasPropertyCallback(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName) {
		const std::string propName = JS2String(propertyName);

		std::string base = "0123456789";
		if (propName.find_first_not_of(base) != std::string::npos) {
			return false;
		}

		size_t index = strTo<size_t>(propName);
		SwigPrivData* t = (SwigPrivData*)JSObjectGetPrivate(object);
		DOMNodeList* nodeList = (DOMNodeList*)t->swigCObject;

		if (nodeList->getLength() < index) {
			return false;
		}

		return true;
	}

	JSValueRef JSCNodeListGetPropertyCallback(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception) {
		const std::string propName = JS2String(propertyName);

		std::string base = "0123456789";
		if (propName.find_first_not_of(base) != std::string::npos) {
			return JSValueMakeUndefined(context);
		}

		size_t index = strTo<size_t>(propName);
		SwigPrivData* t = (SwigPrivData*)JSObjectGetPrivate(object);
		DOMNodeList* nodeList = (DOMNodeList*)t->swigCObject;

		if (nodeList->getLength() < index) {
			return JSValueMakeUndefined(context);
		}

		DOMNode* node = nodeList->item(index);
		JSValueRef jsresult = SWIG_NewPointerObj(SWIG_as_voidptr(node),
			SWIG_TypeDynamicCast(SWIGTYPE_p_XERCES_CPP_NAMESPACE__DOMNode, SWIG_as_voidptrptr(&node)), 0);
		return jsresult;
	}
#endif

	std::shared_ptr<DataModelImpl> JSCDataModel::create(DataModelCallbacks* callbacks) {
		std::shared_ptr<JSCDataModel> dm(new JSCDataModel());
		dm->_callbacks = callbacks;
		dm->setup();
		return dm;
	}

	void JSCDataModel::setup() {
		_ctx = JSGlobalContextCreate(NULL);

#ifndef NO_XERCESC
		JSObjectRef exports;

		// register subscript operator with nodelist
		_exports_DOMNodeList_objectDefinition.hasProperty = JSCNodeListHasPropertyCallback;
		_exports_DOMNodeList_objectDefinition.getProperty = JSCNodeListGetPropertyCallback;

		// not thread safe!
		{
			std::lock_guard<std::mutex> lock(_initMutex);
			JSCDOM_initialize(_ctx, &exports);
		}
#endif

		// introduce global functions as objects for private data
		JSClassRef jsInClassRef = JSClassCreate(&jsInClassDef);
		JSObjectRef jsIn = JSObjectMake(_ctx, jsInClassRef, this);
		JSStringRef inName = JSStringCreateWithUTF8CString("In");
		JSObjectSetProperty(_ctx, JSContextGetGlobalObject(_ctx), inName, jsIn, kJSPropertyAttributeNone, NULL);
		JSStringRelease(inName);

		JSClassRef jsPrintClassRef = JSClassCreate(&jsPrintClassDef);
		JSObjectRef jsPrint = JSObjectMake(_ctx, jsPrintClassRef, this);
		JSStringRef printName = JSStringCreateWithUTF8CString("print");
		JSObjectSetProperty(_ctx, JSContextGetGlobalObject(_ctx), printName, jsPrint, kJSPropertyAttributeNone, NULL);
		JSStringRelease(printName);

		JSClassRef jsInvokerClassRef = JSClassCreate(&jsInvokersClassDef);
		JSObjectRef jsInvoker = JSObjectMake(_ctx, jsInvokerClassRef, this);
		JSStringRef invokerName = JSStringCreateWithUTF8CString("_invokers");
		JSObjectSetProperty(_ctx, JSContextGetGlobalObject(_ctx), invokerName, jsInvoker, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, NULL);
		JSStringRelease(invokerName);

		JSClassRef jsIOProcClassRef = JSClassCreate(&jsIOProcessorsClassDef);
		JSObjectRef jsIOProc = JSObjectMake(_ctx, jsIOProcClassRef, this);
		JSStringRef ioProcName = JSStringCreateWithUTF8CString("_ioprocessors");
		JSObjectSetProperty(_ctx, JSContextGetGlobalObject(_ctx), ioProcName, jsIOProc, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, NULL);
		JSStringRelease(ioProcName);

		JSStringRef nameName = JSStringCreateWithUTF8CString("_name");
		JSStringRef name = JSStringCreateWithUTF8CString(uscxml::fromLocaleToUtf8(_callbacks->getName()).c_str());
		JSObjectSetProperty(_ctx, JSContextGetGlobalObject(_ctx), nameName, JSValueMakeString(_ctx, name), kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, NULL);
		JSStringRelease(nameName);
		JSStringRelease(name);

		JSStringRef sessionIdName = JSStringCreateWithUTF8CString("_sessionid");
		JSStringRef sessionId = JSStringCreateWithUTF8CString(uscxml::fromLocaleToUtf8(_callbacks->getSessionId()).c_str());
		JSObjectSetProperty(_ctx, JSContextGetGlobalObject(_ctx), sessionIdName, JSValueMakeString(_ctx, sessionId), kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, NULL);
		JSStringRelease(sessionIdName);
		JSStringRelease(sessionId);

		evalAsValue("_x = {};");
	}

	void JSCDataModel::setEvent(const Event& event) {
		Event* evPtr = new Event(event);

		JSObjectRef eventObj = SWIG_JSC_NewPointerObj(_ctx, evPtr, SWIGTYPE_p_uscxml__Event, SWIG_POINTER_OWN);
		JSObjectRef globalObject = JSContextGetGlobalObject(_ctx);

		JSValueRef exception = NULL;

		/* Manually handle swig ignored fields */
		EVENT_STRING_OR_UNDEF(sendid, !event.hideSendId); // test333
		EVENT_STRING_OR_UNDEF(origin, event.origin.size() > 0); // test335
		EVENT_STRING_OR_UNDEF(origintype, event.origintype.size() > 0); // test337
		EVENT_STRING_OR_UNDEF(invokeid, event.invokeid.size() > 0); // test339

		/* Manually handle swig ignored event type */
		JSStringRef eventTypeName = JSStringCreateWithUTF8CString("type");
		JSStringRef eventTypeVal;

		// test 331
		const std::string sEventType = Event::TypeToString(event.eventType);
		if (!sEventType.empty())
			eventTypeVal = JSStringCreateWithUTF8CString(sEventType.c_str());

		JSObjectSetProperty(_ctx, eventObj, eventTypeName, JSValueMakeString(_ctx, eventTypeVal), 0, &exception);
		if (exception)
			handleException(exception, "event.eventType:[" + sEventType + "]");

		JSStringRelease(eventTypeName);
		JSStringRelease(eventTypeVal);

		/* Manually handle swig ignored event data */
		if (event.data.node) {
#ifndef NO_XERCESC
			JSStringRef propName = JSStringCreateWithUTF8CString("data");
			JSObjectSetProperty(_ctx, eventObj, propName, getNodeAsValue(event.data.node), 0, &exception);
			JSStringRelease(propName);
			if (exception)
				handleException(exception, "event.data.node:[" + event.data.asJSON() + "]");
#else
			ERROR_EXECUTION_THROW("Compiled without DOM support");
#endif
#if 0
		}
		else if (event.content.length() > 0) {
			// _event.data is a string or JSON
			Data json = Data::fromJSON(event.content);
			if (!json.empty()) {
				JSStringRef propName = JSStringCreateWithUTF8CString("data");
				JSObjectSetProperty(_ctx, eventObj, propName, getDataAsValue(json), 0, &exception);
				JSStringRelease(propName);
				if (exception)
					handleException(exception);
			}
			else {
				JSStringRef propName = JSStringCreateWithUTF8CString("data");
				JSStringRef contentStr = JSStringCreateWithUTF8CString(spaceNormalize(event.content).c_str());
				JSObjectSetProperty(_ctx, eventObj, propName, JSValueMakeString(_ctx, contentStr), 0, &exception);
				JSStringRelease(propName);
				JSStringRelease(contentStr);

				if (exception)
					handleException(exception);
			}
#endif
		}
		else {
			// _event.data is KVP
			Event eventCopy(event);
			if (!eventCopy.params.empty()) {
				Event::params_t::iterator paramIter = eventCopy.params.begin();
				while (paramIter != eventCopy.params.end()) {
					eventCopy.data.compound[paramIter->first] = paramIter->second;
					paramIter++;
				}
			}
			if (!eventCopy.namelist.empty()) {
				Event::namelist_t::iterator nameListIter = eventCopy.namelist.begin();
				while (nameListIter != eventCopy.namelist.end()) {
					eventCopy.data.compound[nameListIter->first] = nameListIter->second;
					nameListIter++;
				}
			}
			if (!eventCopy.data.empty()) {
				JSStringRef propName = JSStringCreateWithUTF8CString("data");
				JSObjectSetProperty(_ctx, eventObj, propName, getDataAsValue(eventCopy.data), 0, &exception);
				JSStringRelease(propName);
				if (exception)
					handleException(exception, "event.data:[" + eventCopy.data.asJSON() + "]");
			}
			else {
				// test 343 / test 488
				JSStringRef propName = JSStringCreateWithUTF8CString("data");
				JSObjectSetProperty(_ctx, eventObj, propName, JSValueMakeUndefined(_ctx), 0, &exception);
				JSStringRelease(propName);
				if (exception)
					handleException(exception, "event.data:[undefined]");
			}
		}
		JSStringRef eventName = JSStringCreateWithUTF8CString("_event");
		JSObjectSetProperty(_ctx, globalObject, eventName, eventObj, kJSPropertyAttributeDontDelete, &exception);
		JSStringRelease(eventName);
		if (exception)
			handleException(exception, "_event");

	}

	Data JSCDataModel::getAsData(const std::string& content) {
		Data d;
		try {
			/* SCXML-tutorial\Tests\ecma\Custom\types\testConditionalExpressions.scxml */
			d = evalAsData(content);
		}
		catch (uscxml::ErrorEvent &) {
			// parse as JSON test578
			d = Data::fromJSON(content);
			if (!d.empty()) {
				return d;
			}

			std::string trimmed = boost::trim_copy(content);
			if (trimmed.length() > 0) {
				if (isNumeric(trimmed.c_str(), 10)) {
					d = Data(trimmed, Data::INTERPRETED);
				}
				else if (trimmed.length() >= 2 &&
					((trimmed[0] == '"' && trimmed[trimmed.length() - 1] == '"') ||
					(trimmed[0] == '\'' && trimmed[trimmed.length() - 1] == '\''))) {
					d = Data(trimmed.substr(1, trimmed.length() - 2), Data::VERBATIM);
				}
				else {
					// test558, test562
					ERROR_EXECUTION(e, "Given content cannot be interpreted as data");
					e.data.compound["context"] = Data(trimmed, Data::VERBATIM);
					throw e;
				}
			}
		}		
		return d;
	}

	JSValueRef JSCDataModel::getDataAsValue(const Data& data) {
		JSValueRef exception = NULL;

		if (data.node) {
#ifndef NO_XERCESC
			return getNodeAsValue(data.node);
#else
			ERROR_EXECUTION_THROW("Compiled without DOM support");
#endif

		}
		if (data.compound.size() > 0) {
			JSObjectRef value = JSObjectMake(_ctx, 0, 0);
			std::map<std::string, Data>::const_iterator compoundIter = data.compound.begin();
			while (compoundIter != data.compound.end()) {
				JSStringRef key = JSStringCreateWithUTF8CString(uscxml::fromLocaleToUtf8(compoundIter->first).c_str());
				JSObjectSetProperty(_ctx, value, key, getDataAsValue(compoundIter->second), 0, &exception);
				JSStringRelease(key);
				if (exception)
					handleException(exception, "data.compound:[" + data.asJSON() + "]");
				compoundIter++;
			}
			return value;
		}
		if (data.array.size() > 0) {
			std::vector<JSValueRef> elements(data.array.size());
			int count = 0;
			for (const auto &arrayIter : data.array) {
				auto ref = getDataAsValue(arrayIter.second);
				elements[count++] = ref;				
			}

			JSObjectRef value = JSObjectMakeArray(_ctx, data.array.size(), elements.data(), &exception);
			if (exception)
				handleException(exception, "data.array:[" + data.asJSON() + "]");
			return value;
		}
		if (data.atom.size() > 0) {
			switch (data.type) {
			case Data::VERBATIM: {
				JSStringRef stringRef = JSStringCreateWithUTF8CString(uscxml::fromLocaleToUtf8(data.atom).c_str());
				JSValueRef value = JSValueMakeString(_ctx, stringRef);
				JSStringRelease(stringRef);
				return value;
				break;
			}
			case Data::INTERPRETED: {
				return evalAsValue(data.atom);
				break;
			}
			}
		}

		return JSValueMakeUndefined(_ctx);
	}

	Data JSCDataModel::getValueAsData(const JSValueRef value) {
		Data data;
		JSValueRef exception = NULL;
		switch (JSValueGetType(_ctx, value)) {
		case kJSTypeUndefined: {
			data.atom = "undefined";
			data.type = Data::INTERPRETED;
		} break;
		case kJSTypeNull: {
			data.atom = "null";
			data.type = Data::INTERPRETED;
		} break;
		case kJSTypeBoolean: {
			data.atom = (JSValueToBoolean(_ctx, value) ? "true" : "false");
		} break;
		case kJSTypeNumber: {
			data.atom = toStr(JSValueToNumber(_ctx, value, &exception));
			if (exception)
				handleException(exception);
		} break;
		case kJSTypeString: {
			JSStringRef stringValue = JSValueToStringCopy(_ctx, value, &exception);
			if (exception)
				handleException(exception);

			data.atom = JS2StringAndRelease(stringValue);
			data.type = Data::VERBATIM;
		} break;
		case kJSTypeObject: {
			JSObjectRef objValue = JSValueToObject(_ctx, value, &exception);
			if (exception)
				handleException(exception);

#ifndef NO_XERCESC
			if (JSValueIsObjectOfClass(_ctx, value, _exports_DOMNode_classRef)) {
				// dom node
				void* privData = NULL;
				SWIG_JSC_ConvertPtr(_ctx, value, &privData, SWIGTYPE_p_XERCES_CPP_NAMESPACE__DOMNode, 0);
				data.node = (XERCESC_NS::DOMNode*)privData;
				return data;
			}
#endif
			if (JSObjectIsFunction(_ctx, objValue)) {
				JSStringRef stringValue = JSValueToStringCopy(_ctx, value, &exception);
				if (exception)
					handleException(exception);

				data.atom = JS2StringAndRelease(stringValue);
				data.type = Data::INTERPRETED;
				return data;
			}

			std::set<std::string> propertySet;

			JSPropertyNameArrayRef properties = JSObjectCopyPropertyNames(_ctx, objValue);
			size_t paramCount = JSPropertyNameArrayGetCount(properties);
			bool isArray = true;
			for (size_t i = 0; i < paramCount; i++) {
				JSStringRef stringValue = JSPropertyNameArrayGetNameAtIndex(properties, i);
				const std::string property = JS2String(stringValue);
				if (!isInteger(property.c_str(), 10))
					isArray = false;
				propertySet.insert(property);
			}
			JSPropertyNameArrayRelease(properties);
			std::set<std::string>::iterator propIter = propertySet.begin();
			while (propIter != propertySet.end()) {
				if (isArray) {
					const int propIndex = strTo<int>(*propIter);
					JSValueRef nestedValue = JSObjectGetPropertyAtIndex(_ctx, objValue, propIndex, &exception);
					if (exception)
						handleException(exception);
					data.array.insert(std::make_pair(propIndex, getValueAsData(nestedValue)));
				}
				else {
					JSStringRef jsString = JSStringCreateWithUTF8CString(uscxml::fromLocaleToUtf8(*propIter).c_str());
					JSValueRef nestedValue = JSObjectGetProperty(_ctx, objValue, jsString, &exception);
					JSStringRelease(jsString);
					if (exception)
						handleException(exception, "property:[" + *propIter + "]");
					data.compound[*propIter] = getValueAsData(nestedValue);
				}
				propIter++;
			}

			// consider that we have data as empty object '{}' or complex object like 'new Date()'
			if (data.empty()) {
				throw JSComplexClassException();
			}
		} break;
		}
		return data;
	}

	uint32_t JSCDataModel::getLength(const std::string& expr) {
		JSValueRef result;

		result = evalAsValue("(" + expr + ").length");
		JSType type = JSValueGetType(_ctx, result);
		if (type == kJSTypeNull || type == kJSTypeUndefined) {
			ERROR_EXECUTION_THROW("'" + expr + "' does not evaluate to an array.");
		}

		JSValueRef exception = NULL;
		double length = JSValueToNumber(_ctx, result, &exception);
		if (exception)
			handleException(exception, "getLength:[" + expr + "]");

		return (uint32_t)length;
	}

	void JSCDataModel::setForeach(const std::string& item,
		const std::string& array,
		const std::string& index,
		uint32_t iteration) {
		if (!isDeclared(item)) {
			assign(item, Data("null", Data::INTERPRETED));
		}
		// assign array element to item
		std::stringstream ss;
		ss << array << "[" << iteration << "]";
		assign(item, Data(ss.str(), Data::INTERPRETED));
		if (index.length() > 0) {
			// assign iteration element to index
			std::stringstream ss;
			ss << iteration;
			assign(index, Data(ss.str(), Data::INTERPRETED));
		}
	}

	bool JSCDataModel::isValidExprSyntax(const std::string& expr) {
		return isValidScriptSyntax("var __tmp=" + boost::trim_copy(expr) + ";");
	}

	bool JSCDataModel::isValidScriptSyntax(const std::string & script)
	{
		JSStringRef scriptJS = JSStringCreateWithUTF8CString(uscxml::fromLocaleToUtf8(script).c_str());
		JSValueRef exception = NULL;
		bool valid = JSCheckScriptSyntax(_ctx, scriptJS, NULL, 0, &exception);
		JSStringRelease(scriptJS);

		if (exception) {
			try {
				handleException(exception, script);
			}
			catch (uscxml::ErrorEvent &e) {
				LOG(_callbacks->getLogger(), USCXML_ERROR) << e << std::endl;
			}
			return false;
		}

		if (!valid)
			return false;

		return true;
	}

	bool JSCDataModel::isDeclared(const std::string& expr) {
		JSStringRef scriptJS = JSStringCreateWithUTF8CString(uscxml::fromLocaleToUtf8(expr).c_str());
		JSValueRef exception = NULL;
		JSValueRef result = JSEvaluateScript(_ctx, scriptJS, NULL, NULL, 0, &exception);
		JSStringRelease(scriptJS);

		if (exception || JSValueIsNull(_ctx, result)) {
			return false;
		}
		return true;
	}

	bool JSCDataModel::evalAsBool(const std::string& expr) {
		JSValueRef result = evalAsValue(expr);
		return JSValueToBoolean(_ctx, result);
	}

	void JSCDataModel::eval(const std::string & content) {
		evalAsValue(content);
	}

	Data JSCDataModel::evalAsData(const std::string& content) {
		try {
			if (!content.empty()) {
				try {
					JSValueRef result = evalAsValue("var __tmp=function() { return(" + content + "); }; __tmp();");

					return getValueAsData(result);
				}
				catch (JSComplexClassException &) {
					return Data(content, Data::INTERPRETED);
				}
			}		
		}
		catch (ErrorEvent e) {
			// test453 vs test554
			throw e;			
		}
		return Data("undefined", Data::INTERPRETED);
	}

	JSValueRef JSCDataModel::evalAsValue(const std::string& expr, bool dontThrow) {
		// test326
		if (expr.empty())
			return JSValueMakeUndefined(_ctx);

		JSStringRef scriptJS = JSStringCreateWithUTF8CString(uscxml::fromLocaleToUtf8(expr).c_str());
		JSValueRef exception = NULL;
		JSValueRef result = JSEvaluateScript(_ctx, scriptJS, NULL, NULL, 0, &exception);
		JSStringRelease(scriptJS);

		if (exception && !dontThrow)
			handleException(exception, expr);

		return result;
	}

#ifndef NO_XERCESC
	JSValueRef JSCDataModel::getNodeAsValue(const DOMNode* node) {
		return SWIG_JSC_NewPointerObj(_ctx,
			(void*)node,
			SWIG_TypeDynamicCast(SWIGTYPE_p_XERCES_CPP_NAMESPACE__DOMNode,
				SWIG_as_voidptrptr(&node)),
			0);

	}
#endif

	void JSCDataModel::assign(const std::string& location, const Data& data, const std::map<std::string, std::string>& attr) {

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

		JSValueRef exception = NULL;
		if (data.node) {
#ifndef NO_XERCESC
			JSObjectSetProperty(_ctx, JSContextGetGlobalObject(_ctx), JSStringCreateWithUTF8CString(uscxml::fromLocaleToUtf8(location).c_str()), getNodeAsValue(data.node), 0, &exception);
#else
			ERROR_EXECUTION_THROW("Compiled without DOM support");
#endif
		}
		else {
			
			if (data.type == Data::VERBATIM && !data.atom.empty() && isValidExprSyntax(data.atom)) {
				evalAsValue(location + "=" + data.atom);
			}
			else {
				evalAsValue(location + "=" + Data::toJSON(data));
			}
		}

		if (exception)
			handleException(exception, "location:[" + location + "] data:[" + data.asJSON() + "]");
	}

	void JSCDataModel::init(const std::string& location, const Data& data, const std::map<std::string, std::string>& attr) {
		try {
			if (data.empty()) {
				assign(location, Data("null", Data::INTERPRETED));
			}
			else {
				assign(location, data);
			}
		}
		catch (ErrorEvent e) {
			// test 277
			evalAsValue(location + " = undefined", true);
			throw e;
		}
	}

	void JSCDataModel::handleException(JSValueRef exception, const std::string &description /*=""*/) {

		std::string sLine = "";

		JSValueRef eObj = nullptr;
		JSObjectRef objRef = JSValueToObject(_ctx, exception, &eObj);
		if (!eObj && objRef) {
#if 0 // deep debug
			DumpJSObject(_callbacks->getLogger(), _ctx, objRef);
#endif
			JSValueRef eProp = nullptr;
			JSStringRef jsPropName = JSStringCreateWithUTF8CString("line");
			JSValueRef jsPropValue = JSObjectGetProperty(_ctx, objRef, jsPropName, &eProp);
			JSStringRelease(jsPropName);
			if (!eProp && jsPropValue) {
				sLine = JS2StringAndRelease(JSValueToStringCopy(_ctx, jsPropValue, NULL)) + ":";
			}
		}

		uscxml::ErrorEvent e;
		e.data.compound["cause"] = uscxml::Data(sLine + JS2StringAndRelease(JSValueToStringCopy(_ctx, exception, NULL)), uscxml::Data::VERBATIM);
		e.name = "error.execution";
		e.eventType = uscxml::Event::PLATFORM;

		if (!_callbacks->getName().empty()) {
			e.data.compound["interpreter"] = Data(_callbacks->getName(), Data::VERBATIM);
		}
		if (!description.empty()) {
			e.data.compound["description"] = Data(description, Data::VERBATIM);
		}
		throw e;
	}

	JSValueRef JSCDataModel::jsPrint(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
		JSCDataModel* INSTANCE = (JSCDataModel*)JSObjectGetPrivate(function);

		for (unsigned int i = 0; i < argumentCount; i++) {
			if (JSValueIsString(ctx, arguments[i])) {
				JSStringRef stringRef = JSValueToStringCopy(ctx, arguments[i], exception);
				if (*exception)
					INSTANCE->handleException(*exception, "jsPrint arg:" + std::to_string(i));

				INSTANCE->_callbacks->getLogger().log(USCXML_LOG) << JS2StringAndRelease(stringRef);
			}
		}
		return JSValueMakeUndefined(ctx);
	}

	JSValueRef JSCDataModel::jsIn(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
		JSCDataModel* INSTANCE = (JSCDataModel*)JSObjectGetPrivate(function);

		for (unsigned int i = 0; i < argumentCount; i++) {
			if (JSValueIsString(ctx, arguments[i])) {
				JSStringRef stringRef = JSValueToStringCopy(ctx, arguments[i], exception);
				if (*exception)
					INSTANCE->handleException(*exception);

				if (INSTANCE->_callbacks->isInState(JS2StringAndRelease(stringRef))) {
					continue;
				}
			}
			return JSValueMakeBoolean(ctx, false);
		}
		return JSValueMakeBoolean(ctx, true);

	}

	bool JSCDataModel::jsIOProcessorHasProp(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName) {
		JSCDataModel* INSTANCE = (JSCDataModel*)JSObjectGetPrivate(object);
		std::map<std::string, IOProcessor> ioProcessors = INSTANCE->_callbacks->getIOProcessors();

		const std::string prop = JS2String(propertyName);

		return ioProcessors.find(prop) != ioProcessors.end();
	}

	JSValueRef JSCDataModel::jsIOProcessorGetProp(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception) {
		JSCDataModel* INSTANCE = (JSCDataModel*)JSObjectGetPrivate(object);
		std::map<std::string, IOProcessor> ioProcessors = INSTANCE->_callbacks->getIOProcessors();

		const std::string prop = JS2String(propertyName);

		auto it = ioProcessors.find(prop);
		if (it != ioProcessors.end()) {
			return INSTANCE->getDataAsValue(it->second.getDataModelVariables());
		}
		return JSValueMakeUndefined(ctx);
	}

	void JSCDataModel::jsIOProcessorListProps(JSContextRef ctx, JSObjectRef object, JSPropertyNameAccumulatorRef propertyNames) {
		JSCDataModel* INSTANCE = (JSCDataModel*)JSObjectGetPrivate(object);
		std::map<std::string, IOProcessor> ioProcessors = INSTANCE->_callbacks->getIOProcessors();

		std::map<std::string, IOProcessor>::const_iterator ioProcIter = ioProcessors.begin();
		while (ioProcIter != ioProcessors.end()) {
			JSStringRef ioProcName = JSStringCreateWithUTF8CString(uscxml::fromLocaleToUtf8(ioProcIter->first).c_str());
			JSPropertyNameAccumulatorAddName(propertyNames, ioProcName);
			ioProcIter++;
		}
	}


	bool JSCDataModel::jsInvokerHasProp(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName) {
		JSCDataModel* INSTANCE = (JSCDataModel*)JSObjectGetPrivate(object);
		std::map<std::string, Invoker> invokers = INSTANCE->_callbacks->getInvokers();

		const std::string prop = JS2String(propertyName);

		return invokers.find(prop) != invokers.end();
	}

	JSValueRef JSCDataModel::jsInvokerGetProp(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception) {
		JSCDataModel* INSTANCE = (JSCDataModel*)JSObjectGetPrivate(object);
		std::map<std::string, Invoker> invokers = INSTANCE->_callbacks->getInvokers();

		const std::string prop = JS2String(propertyName);

		if (invokers.find(prop) != invokers.end()) {
			return INSTANCE->getDataAsValue(invokers.find(prop)->second.getDataModelVariables());
		}
		return JSValueMakeUndefined(ctx);
	}

	void JSCDataModel::jsInvokerListProps(JSContextRef ctx, JSObjectRef object, JSPropertyNameAccumulatorRef propertyNames) {
		JSCDataModel* INSTANCE = (JSCDataModel*)JSObjectGetPrivate(object);
		std::map<std::string, Invoker> invokers = INSTANCE->_callbacks->getInvokers();

		std::map<std::string, Invoker>::const_iterator invokerIter = invokers.begin();
		while (invokerIter != invokers.end()) {
			JSStringRef invokeName = JSStringCreateWithUTF8CString(uscxml::fromLocaleToUtf8(invokerIter->first).c_str());
			JSPropertyNameAccumulatorAddName(propertyNames, invokeName);
			JSStringRelease(invokeName);
			invokerIter++;
		}
	}

}
