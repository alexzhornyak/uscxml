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

#include <vector>

#include "uscxml/messages/Data.h"
#include "uscxml/messages/Blob.h"

#include <boost/algorithm/string.hpp>

#ifndef NO_XERCESC
#include "uscxml/util/DOM.h"
#endif

#include "uscxml/interpreter/Logging.h"

#ifdef HAS_STRING_H
#include <string.h>
#endif

extern "C" {
#include "jsmn/jsmn.h" // minimal json parser
}

namespace uscxml {

Data::Data(const char* data, size_t size, const std::string& mimeType, bool adopt) : node(NULL), binary(data, size, mimeType, adopt) {}

void Data::merge(const Data& other) {
	if (other.compound.size() > 0) {
		if (compound.size() == 0) {
			compound = other.compound;
		} else {
			std::map<std::string, Data>::const_iterator compIter = other.compound.begin();
			while (compIter !=  other.compound.end()) {
				if (compound.find(compIter->first) != compound.end()) {
					// we do have the same key, merge
					compound[compIter->first].merge(compIter->second);
				} else {
					compound[compIter->first] = compIter->second;
				}
				compIter++;
			}
		}
	}
	if (other.array.size() > 0) {
		if (array.size() == 0) {
			array = other.array;
		} else {
			array.insert(other.array.begin(), other.array.end());
		}
	}
	if (other.atom.size() > 0) {
		atom = other.atom;
		type = other.type;
	}
}

Data Data::fromJSON(const std::string& jsonString) {

	Data data;

	std::string trimmed = boost::trim_copy(jsonString);

	if (trimmed.length() == 0)
		return data;

	if (trimmed.find_first_of("{[") != 0) {
		/* 24.04.2019 */
		// What about the next example:
		// Data d;
		// d.atom = 5;
		// std::string tmp = d.toJSON();
		// Data d2 = fromJSON(tmp);
		// we expect 'd==d2', so return 'jsonString' as is
		data.atom = jsonString;
		/* 10.02.2021 */
		// test294
		if (!isNumeric(data.atom.c_str(), 10)) {
			data.setType(Data::VERBATIM);
		}
		
		return data;
	}
		

	jsmn_parser p;

	std::vector<jsmntok_t> t;

	// we do not know the number of tokens beforehand, start with something sensible and increase
	int rv;
	int frac = 16; // length/token ratio
	do {
		jsmn_init(&p);

		frac /= 2;
		int nrTokens = trimmed.size() / frac;
		t.clear();
		t.resize((nrTokens + 1) * sizeof(jsmntok_t));
		memset(&t[0], 0, t.size() * sizeof(t[0]));
		rv = jsmn_parse(&p, trimmed.c_str(), t.data(), nrTokens);
	} while (rv == JSMN_ERROR_NOMEM && frac > 1);

	if (rv != 0) {
		switch (rv) {
		case JSMN_ERROR_NOMEM: {
			ERROR_PLATFORM_THROW("Cannot parse JSON, not enough tokens were provided! Data:[" + trimmed + "]"); 
			break;
		}
		case JSMN_ERROR_INVAL: {
			ERROR_PLATFORM_THROW("Cannot parse JSON, invalid character inside JSON string! Data:[" + trimmed + "]");
			break;
		}
		case JSMN_ERROR_PART: {
			ERROR_PLATFORM_THROW("Cannot parse JSON, the string is not a full JSON packet, more bytes expected! Data:[" + trimmed + "]");
			break;
		}
		default:
			break;
		}
		t.clear();
		return data;
	}

	if (t.size()==0 || static_cast<size_t>(t[0].end) != trimmed.length())
		return data;

	std::list<Data*> dataStack;
	std::list<jsmntok_t> tokenStack;
	dataStack.push_back(&data);

	size_t currTok = 0;
	int index = 0;
	do {
		switch (t[currTok].type) {
		case JSMN_STRING:
			dataStack.back()->type = Data::VERBATIM;
		case JSMN_PRIMITIVE: {
			std::string value = trimmed.substr(t[currTok].start, t[currTok].end - t[currTok].start);
			value = jsonUnescape(value);
			dataStack.back()->atom = value;
			dataStack.pop_back();
			currTok++;
			break;
		}
		case JSMN_OBJECT:
		case JSMN_ARRAY:
			tokenStack.push_back(t[currTok]);
			currTok++;
			break;
		}

		// there are no more tokens
		if (t[currTok].end == 0 || tokenStack.empty())
			break;

		// next token starts after current one => pop
		while (t[currTok].end > tokenStack.back().end) {
			tokenStack.pop_back();
			dataStack.pop_back();
		}

		if (tokenStack.back().type == JSMN_OBJECT && (t[currTok].type == JSMN_PRIMITIVE || t[currTok].type == JSMN_STRING)) {
			// grab key and push new data
			std::string value = jsonUnescape(trimmed.substr(t[currTok].start, t[currTok].end - t[currTok].start));
			dataStack.push_back(&(dataStack.back()->compound[value]));
			currTok++;
		}
		if (tokenStack.back().type == JSMN_ARRAY) {
			// push new index
			dataStack.back()->array.insert(std::make_pair(index,Data()));
			dataStack.push_back(&dataStack.back()->array[index]);
			index++;
		}

	} while (true);

	t.clear();
	return data;
}

std::ostream& operator<< (std::ostream& os, const Data& data) {
	os << data.asJSON();
	return os;
}

std::string Data::asJSON() const {
	return Data::toJSON(*this);
}

std::string Data::toJSON(const Data& data) {
	std::stringstream os;
	std::string indent;
	for (size_t i = 0; i <= _dataIndentation; i++) {
		indent += "  ";
	}
	if (false) {
	} else if (data.compound.size() > 0) {
		size_t longestKey = 0;
		std::map<std::string, Data>::const_iterator compoundIter = data.compound.begin();
		while(compoundIter != data.compound.end()) {
			if (compoundIter->first.size() > longestKey)
				longestKey = compoundIter->first.size();
			compoundIter++;
		}
		std::string keyPadding;
		for (size_t i = 0; i < longestKey; i++)
			keyPadding += " ";

		std::string seperator;
		os << "{";
		compoundIter = data.compound.begin();
		while(compoundIter != data.compound.end()) {
			os << seperator << std::endl << indent << "  \"" << jsonEscape(compoundIter->first) << "\": " << keyPadding.substr(0, longestKey - compoundIter->first.size());
			_dataIndentation += 1;
			os << compoundIter->second;
			_dataIndentation -= 1;
			seperator = ", ";
			compoundIter++;
		}
		os << std::endl << indent << "}";
	} else if (data.array.size() > 0) {

		std::string seperator;
		os << std::endl << indent << "[";
		for (auto it : data.array) {
			_dataIndentation += 1;
			os << seperator << it.second;
			_dataIndentation -= 1;
			seperator = ", ";
		}

		os << "]";
	} else if (data.atom.size() > 0) {
		// empty string is handled below
		if (data.type == Data::VERBATIM) {
			os << "\"" << jsonEscape(data.atom) << "\"";
		} else {
			os << data.atom;
		}
#ifndef NO_XERCESC
	} else if (data.node) {
		std::ostringstream xmlSerSS;
		xmlSerSS << *data.node;
		std::string xmlSer = xmlSerSS.str();
//		boost::replace_all(xmlSer, "\"", "\\\"");
//		boost::replace_all(xmlSer, "\n", "\\n");
//		boost::replace_all(xmlSer, "\t", "\\t");
		os << "\"" << jsonEscape(xmlSer) << "\"";
#endif
	} else {
		if (data.type == Data::VERBATIM) {
			os << "\"\""; // empty string
		} else {
			os << "null"; // non object
		}
	}
	return os.str();
}

std::string Data::jsonUnescape(const std::string& expr) {

	// http://stackoverflow.com/a/19636328/990120
	bool escape = false;
	std::string output;
	output.reserve(expr.length());

	for (std::string::size_type i = 0; i < expr.length(); ++i) {
		if (escape) {
			switch(expr[i])  {
			case '"':
				output += '\"';
				break;
			case '/':
				output += '/';
				break;
			case 'b':
				output += '\b';
				break;
			case 'f':
				output += '\f';
				break;
			case 'n':
				output += '\n';
				break;
			case 'r':
				output += '\r';
				break;
			case 't':
				output += '\t';
				break;
			case '\\':
				output += '\\';
				break;
			default:
				output += expr[i];
				break;
			}
			escape = false;
		} else {
			switch(expr[i]) {
			case '\\':
				escape = true;
				break;
			default:
				output += expr[i];
				break;
			}
		}
	}
	return output;

}

std::string Data::jsonEscape(const std::string& expr) {
	std::stringstream os;
	for (size_t i = 0; i < expr.size(); i++) {
		// escape string
		if (false) {
		} else if (expr[i] == '\t') {
			os << "\\t";
		} else if (expr[i] == '\v') {
			os << "\\v";
		} else if (expr[i] == '\b') {
			os << "\\b";
		} else if (expr[i] == '\f') {
			os << "\\f";
		} else if (expr[i] == '\n') {
			os << "\\n";
		} else if (expr[i] == '\r') {
			os << "\\r";
//		} else if (expr[i] == '\'') {
//			os << "\\\'";
		} else if (expr[i] == '\"') {
			os << "\\\"";
		} else if (expr[i] == '\\') {
			os << "\\\\";
		} else {
			os << expr[i];
		}
	}

	return os.str();
}
}
