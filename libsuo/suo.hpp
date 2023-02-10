#pragma once

#include <cmath>
#include <string>
#include <vector>
#include <exception>
#include <iostream>
#include <map>

#include "base_types.hpp"
#include "vectors.hpp"
#include "frame.hpp"
#include "port.hpp"

namespace suo
{

const float pi2f = 6.283185307179586f;

/*
 * Suo's general exception class
 */
class SuoError : public std::exception {
public:
	SuoError(const char* format, ...);
	virtual const char* what() const throw() { return error_msg; }
	virtual ~SuoError() throw() {};
private:
	char error_msg[512];
};


/* Suo message flags */
#define SUO_FLAGS_HAS_TIMESTAMP  0x0100
#define SUO_FLAGS_TX_ACTIVE      0x0200
#define SUO_FLAGS_RX_ACTIVE      0x0400
#define SUO_FLAGS_RX_LOCKED      0x0800


typedef std::map<std::string, std::string> Kwargs;
extern unsigned int rx_id_counter;

/* -----------------------------------------
 * Receive related interfaces and data types
 * ----------------------------------------- */

class Block 
{
public:
	virtual ~Block() = default;
	
	//virtual void setConfig(std::string name, std::string value);

	virtual void sinkFrame(const Frame& frame);
	virtual void sourceFrame(Frame& frame);

	virtual void sinkSymbol(Symbol sym, Timestamp timestamp);
	virtual void sinkSymbols(const std::vector<Symbol> &symbols, Timestamp timestamp);
	virtual void sourceSymbols(SymbolVector &symbols, Timestamp timestamp);

	virtual void sinkSoftSymbol(SoftSymbol sym, Timestamp timestamp);
	virtual void sinkSoftSymbols(const std::vector<SoftSymbol> &sym, Timestamp timestamp);

	virtual void sinkSamples(const SampleVector &samples, Timestamp timestamp);
	virtual void sourceSamples(const SampleVector &samples, Timestamp timestamp);

protected:
	//std::map<std::string,int> conf_map;
};

std::string getISOCurrentTimestamp();


}; // namespace suo

#include "generators.hpp"