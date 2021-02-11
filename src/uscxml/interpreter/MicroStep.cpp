/**
 *  @file
 *  @author     2012-2016 Stefan Radomski (stefan.radomski@cs.tu-darmstadt.de)
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

#include "MicroStep.h"
#include "MicroStepImpl.h"

namespace uscxml {

InterpreterState MicroStep::step(size_t blockMs) {
	return _impl ? _impl->step(blockMs) : USCXML_UNDEF;
}
void MicroStep::reset() {
	if (_impl)
		_impl->reset();
}
bool MicroStep::isInState(const std::string& stateId) {
	return _impl ? _impl->isInState(stateId) : false;
}

std::list<XERCESC_NS::DOMElement*> MicroStep::getConfiguration() {
	return _impl ? _impl->getConfiguration() : std::list<XERCESC_NS::DOMElement*>();
}

void MicroStep::init(XERCESC_NS::DOMElement* scxml) {
	if (_impl)
		_impl->init(scxml);
}

void MicroStep::markAsCancelled() {
	if (_impl)
		_impl->markAsCancelled();
}

void MicroStep::deserialize(const Data& encodedState) {
	if (_impl)
		_impl->deserialize(encodedState);
}

Data MicroStep::serialize() {
	return _impl ? _impl->serialize() : Data();
}

std::shared_ptr<MicroStepImpl> MicroStep::getImpl() const {
	return _impl;
}
}
