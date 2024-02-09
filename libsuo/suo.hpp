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

	virtual void sinkFrame(const Frame& frame, suo::Timestamp now);
	virtual void sourceFrame(Frame& frame, suo::Timestamp now);

	virtual void sinkSymbol(Symbol sym, Timestamp now);
	virtual void sinkSymbols(const SymbolVector &symbols, Timestamp now);
	virtual void sourceSymbols(SymbolVector &symbols, Timestamp now);

	virtual void sinkSoftSymbol(SoftSymbol sym, Timestamp now);
	virtual void sinkSoftSymbols(const std::vector<SoftSymbol> &sym, Timestamp now);

	virtual void sinkSamples(const SampleVector &samples, Timestamp now);
	virtual void sourceSamples(SampleVector &samples, Timestamp now);

protected:
	//std::map<std::string,int> conf_map;
};

std::string getCurrentISOTimestamp();


}; // namespace suo

#include "generators.hpp"