/**
 *  @file
 *  @author     2016 Stefan Radomski (stefan.radomski@cs.tu-darmstadt.de)
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

#ifndef INTERPRETERIMPL_H_2A79C83D
#define INTERPRETERIMPL_H_2A79C83D

#include <memory>
#include <mutex>
#include <list>
#include <map>
#include <string>
#include <limits>

#include "uscxml/Common.h"
#include "uscxml/util/URL.h"
#include "uscxml/plugins/Factory.h"
#include "uscxml/plugins/DataModelImpl.h"
#include "uscxml/plugins/IOProcessorImpl.h"
#include "uscxml/plugins/InvokerImpl.h"
#include "uscxml/plugins/ExecutableContent.h"
#include "uscxml/interpreter/MicroStepImpl.h"
#include "uscxml/interpreter/ContentExecutorImpl.h"
#include "uscxml/interpreter/EventQueue.h"
#include "uscxml/interpreter/EventQueueImpl.h"
//#include "uscxml/util/DOM.h"

namespace uscxml {

class InterpreterMonitor;
class InterpreterIssue;

/**
 * @ingroup interpreter
 * @ingroup impl
 */
class USCXML_API InterpreterImpl :
	public MicroStepCallbacks,
	public DataModelCallbacks,
	public IOProcessorCallbacks,
	public ContentExecutorCallbacks,
	public DelayedEventQueueCallbacks,
	public InvokerCallbacks,
	public std::enable_shared_from_this<InterpreterImpl> {
public:
	enum Binding {
		EARLY = 0,
		LATE = 1
	};

	InterpreterImpl();
	virtual ~InterpreterImpl();

	void cloneFrom(InterpreterImpl* other);
	void cloneFrom(std::shared_ptr<InterpreterImpl> other);

	virtual InterpreterState step(size_t blockMs);
	virtual void reset();///< Reset state machine

	virtual void cancel(); ///< Cancel and finalize state machine

	virtual void deserialize(const std::string& encodedState);
	virtual std::string serialize();

	inline InterpreterState getState() {
		return _state;
	}

	inline std::list<XERCESC_NS::DOMElement*> getConfiguration() {
		return _microStepper.getConfiguration();
	}

	inline void addMonitor(InterpreterMonitor* monitor) {
		_monitors.insert(monitor);
	}

	inline void removeMonitor(InterpreterMonitor* monitor) {
		_monitors.erase(monitor);
	}

	/**
	 MicrostepCallbacks
	 */
	inline virtual Event dequeueInternal() override {
		_currEvent = _internalQueue.dequeue(0);
		if (_currEvent)
			_dataModel.setEvent(_currEvent);
		return _currEvent;
	}
	virtual Event dequeueExternal(size_t blockMs) override;
	virtual bool isTrue(const std::string& expr) override;

	inline virtual void raiseDoneEvent(XERCESC_NS::DOMElement* state, XERCESC_NS::DOMElement* doneData) override {
		_execContent.raiseDoneEvent(state, doneData);
	}

	inline virtual void process(XERCESC_NS::DOMElement* block) override {
		_execContent.process(block);
	}

	virtual bool isMatched(const Event& event, const std::string& eventDesc) override;
	virtual void initData(XERCESC_NS::DOMElement* element) override;

	inline virtual void invoke(XERCESC_NS::DOMElement* invoke) override {
		_execContent.invoke(invoke);
	}

	inline virtual void uninvoke(XERCESC_NS::DOMElement* invoke) override {
		_execContent.uninvoke(invoke);
	}

	inline virtual std::set<InterpreterMonitor*> getMonitors() override {
		return _monitors;
	}

	inline virtual Interpreter getInterpreter() override {
		return Interpreter(shared_from_this());
	}

	inline virtual Data& getCache() override {
		return _cache;
	}

	/**
	 DataModelCallbacks
	 */
	inline virtual const std::string& getName() override {
		return _name;
	}
	inline virtual const std::string& getSessionId() override {
		return _sessionId;
	}
	inline virtual const std::map<std::string, IOProcessor>& getIOProcessors() override {
		return _ioProcs;
	}
	inline virtual const std::map<std::string, Invoker>& getInvokers() override {
		return _invokers;
	}

	inline virtual bool isInState(const std::string& stateId) override {
		return _microStepper.isInState(stateId);
	}
	inline virtual XERCESC_NS::DOMDocument* getDocument() const override {
		return _document;
	}

	/**
	 ContentExecutorCallbacks
	 */

	inline virtual void enqueueInternal(const Event& event) override {
		return _internalQueue.enqueue(event);
	}
	inline virtual void enqueueExternal(const Event& event) override {
		return _externalQueue.enqueue(event);
	}
	inline virtual void enqueueExternalDelayed(const Event& event, size_t delayMs, const std::string& eventUUID) override {
		return _delayQueue.enqueueDelayed(event, delayMs, eventUUID);
	}
	virtual void cancelDelayed(const std::string& eventId) override;

	inline virtual size_t getLength(const std::string& expr) override {
		return _dataModel.getLength(expr);
	}

	inline virtual void setForeach(const std::string& item,
	                        const std::string& array,
	                        const std::string& index,
	                        uint32_t iteration) override {
		return _dataModel.setForeach(item, array, index, iteration);
	}
	inline virtual Data evalAsData(const std::string& expr) override {
		return _dataModel.evalAsData(expr);
	}

	inline virtual void eval(const std::string& content) override {
		_dataModel.eval(content);
	}

	inline virtual Data getAsData(const std::string& expr) override {
		return _dataModel.getAsData(expr);
	}

	virtual bool isValidExprSyntax(const std::string& expr) override {
		return _dataModel.isValidExprSyntax(expr);
	}

	virtual bool isValidScriptSyntax(const std::string &script) override {
		return _dataModel.isValidScriptSyntax(script);
	}

	virtual void assign(const std::string& location, const Data& data, const std::map<std::string, std::string>& attrs) override;

	inline virtual std::string getInvokeId() override {
		return _invokeId;
	}
	inline virtual std::string getBaseURL() override {
		return _baseURL;
	}

	virtual bool checkValidSendType(const std::string& type, const std::string& target) override;
	virtual void invoke(const std::string& type, const std::string& src, bool autoForward, XERCESC_NS::DOMElement* finalize, const Event& invokeEvent) override;
	virtual void uninvoke(const std::string& invokeId) override;
	virtual void enqueue(const std::string& type, const std::string& target, size_t delayMs, const Event& sendEvent) override;

	inline virtual const Event& getCurrentEvent() override {
		return _currEvent;
	}

	inline virtual ExecutableContent createExecutableContent(const std::string& localName, const std::string& nameSpace) override {
		return _factory ? _factory->createExecutableContent(localName, nameSpace, this):
			Factory::getInstance().createExecutableContent(localName, nameSpace, this);
	}


	/**
	 IOProcessorCallbacks
	 */
	virtual void enqueueAtInvoker(const std::string& invokeId, const Event& event)  override;
	virtual void enqueueAtParent(const Event& event)  override;

	/**
	 DelayedEventQueueCallbacks
	 */

	virtual void eventReady(Event& event, const std::string& eventUUID)  override;

	/** --- */

	inline void setActionLanguage(const ActionLanguage& al) {
		if (al.logger) // we intialized _logger as the default logger already
			_logger = al.logger;
		_execContent = al.execContent;
		_microStepper = al.microStepper;
		_dataModel = al.dataModel;
		_internalQueue = al.internalQueue;
		_externalQueue = al.externalQueue;
		_delayQueue = al.delayQueue;
	}

	inline ActionLanguage* getActionLanguage() {
		_al.logger = _logger;
		_al.execContent = _execContent;
		_al.microStepper = _microStepper;
		_al.dataModel = _dataModel;
		_al.internalQueue = _internalQueue;
		_al.externalQueue = _externalQueue;
		_al.delayQueue = _delayQueue;
		return &_al;
	}

	inline void setFactory(Factory* factory) {
		_factory = factory;
	}

	inline virtual Factory* getFactory() {
		return _factory;
	}

	inline virtual Logger getLogger() {
		return _logger;
	}

	static std::map<std::string, std::weak_ptr<InterpreterImpl> > getInstances();

	inline virtual XERCESC_NS::DOMDocument* getDocument() {
		return _document;
	}

protected:
	static void addInstance(std::shared_ptr<InterpreterImpl> instance);

	Binding _binding;
	ActionLanguage _al;

	std::string _sessionId;
	std::string _name;
	std::string _invokeId; // TODO: Never set!

	bool _isInitialized;
	XERCESC_NS::DOMDocument* _document;
	XERCESC_NS::DOMElement* _scxml;

	std::map<std::string, std::tuple<std::string, std::string, std::string> > _delayedEventTargets;

	virtual void init();

	static std::map<std::string, std::weak_ptr<InterpreterImpl> > _instances;
	static std::recursive_mutex _instanceMutex;
	std::recursive_mutex _delayMutex;
	std::recursive_mutex _serializationMutex;

	friend class Interpreter;
	friend class InterpreterIssue;
	friend class TransformerImpl;
	friend class USCXMLInvoker;
	friend class SCXMLIOProcessor;
	friend class DebugSession;
	friend class Debugger;

	std::string _xmlPrefix;
	std::string _xmlNS;
	Factory* _factory;

	URL _baseURL;
	std::string _md5;

	MicroStep _microStepper;
	DataModel _dataModel;
	ContentExecutor _execContent;
	Logger _logger = Logger::getDefault();

	InterpreterState _state;

	EventQueue _internalQueue;
	EventQueue _externalQueue;
	EventQueue _parentQueue;
	DelayedEventQueue _delayQueue;

	Event _currEvent;
	Event _invokeReq;

	std::map<std::string, IOProcessor> _ioProcs;
	std::map<std::string, Invoker> _invokers;
	std::map<std::string, XERCESC_NS::DOMElement*> _finalize;
	std::set<std::string> _autoForwarders;
	std::set<InterpreterMonitor*> _monitors;

	Data _cache;

private:
	void setupDOM();
};

}

#endif /* end of include guard: INTERPRETERIMPL_H_2A79C83D */
