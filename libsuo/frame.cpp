
#include "suo.hpp"
#include "coloring.hpp"

#include "json.hpp"

#include <iomanip>
#include <ctime>
#include "boost/variant.hpp"

using namespace suo;
using namespace std;
using json = nlohmann::json;

Frame::FormattingFlags Frame::default_formatting = Frame::PrintData | Frame::PrintMetadata | Frame::PrintColored;

size_t Frame::printer_data_column_count = 32;


Frame::Frame(size_t data_len) :
	id(0),
	flags(Frame::Flags::none),
	timestamp(0)
{
	data.reserve(data_len);
}


void Frame::clear()
{
	id = 0;
	timestamp = 0;
	flags = Frame::Flags::none;
	metadata.clear();
	data.clear();
}


std::ostream& suo::operator<<(std::ostream& stream, const Metadata& metadata) {
	stream << metadata.first << " = ";
	std::visit([&](auto const& a) { stream << a; }, metadata.second);
	return stream;
}

std::ostream& suo::operator<<(std::ostream& _stream, const Frame& frame) {
	_stream << frame(Frame::default_formatting);
	return _stream;
}

std::ostream& suo::operator<<(std::ostream& _stream, const Frame::Printer& printer)
{
	std::ostream stream(_stream.rdbuf());
	const Frame& frame = printer.frame;
	Frame::FormattingFlags flags = printer.flags;

	if (flags & Frame::PrintColored)
		stream << ((flags & Frame::PrintAltColor) ? clr::cyan : clr::green);

	if (flags & Frame::PrintCompact) {
		stream << left;
		stream << "Frame #" << setw(6) << frame.id << " "; // "% -6u";
		stream << "length: " << setw(3) << frame.data.size() << "bytes, ";
		stream << "timestamp: " << setw(10) << frame.timestamp; // "%-10lu\n",

		if (flags & Frame::PrintColored)
			stream << clr::reset; // Reset color modificator

		return _stream;
	}

	stream << endl << "####" << endl;

	stream << left;
	stream << "Frame #" << setw(6) << frame.id << " ";
	stream << "length: " << setw(1) << frame.data.size() << " bytes, ";
	stream << "timestamp: " << setw(10) << frame.timestamp << endl;


	if (flags & Frame::PrintMetadata) {
		stream << "Metadata: ";
		if (frame.metadata.size() > 0) {
			bool first = true;
			for (auto meta : frame.metadata) {
				if (!first)
					stream << "; ";
				stream << meta;
				first = false;
			}
		}
		else
			stream << "(none)";
		stream << endl;
	}


	if (flags & Frame::PrintData) { // right << setw(2) << internal 
		stream << "Data:" << hex << right << setfill('0');
		for (unsigned int i = 0; i < frame.data.size(); i++) {
			if ((i % Frame::printer_data_column_count) == 0)
				stream << endl << "   ";
			stream << setw(2) << (int)frame.data[i] << " ";
		}   
		stream << endl;
	}

	if (flags & Frame::PrintColored)
		stream << clr::reset; // Reset color modificator

	return _stream;
}

/*
SymbolGenerator Frame::generateSymbols(Frame& frame) {
	for (Byte byte : frame.data) {
#define UNROLLER(i) co_yield (byte & (0x80 >> i)) != 0;
		UNROLLER(0); UNROLLER(1); UNROLLER(2); UNROLLER(3);
		UNROLLER(4); UNROLLER(5); UNROLLER(6); UNROLLER(7);
#undef UNROLLER
	}
}*/


Frame Frame::deserialize_from_json(const std::string& json_string) {

	try {

		/* Parse string to dict */
		json dict = json::parse(json_string);
		if (dict.is_object() == false)
			throw SuoError("Received JSON string was not a dict/object.");

		Frame frame;
		frame.id = dict.value("id", 0);

		if (dict.contains("timestamp")) {
			frame.timestamp = dict["timestamp"];
			frame.flags |= Frame::Flags::has_timestamp;
		}

		if (dict.contains("metadata")) {
			auto metas = dict["metadata"];
			if (metas.is_object() == false)
				throw SuoError("JSON metadata is not a dict/object.");
			for (auto json_meta : metas.items()) {
				auto value = json_meta.value();
				if (value.is_string())
					frame.setMetadata(json_meta.key(), value.get<std::string>());
				else if (value.is_number_integer())
					frame.setMetadata(json_meta.key(), value.get<int>());
				else if (value.is_number_unsigned())
					frame.setMetadata(json_meta.key(), value.get<unsigned int>());
				else if (value.is_number_float())
					frame.setMetadata(json_meta.key(), value.get<float>());
				else
					throw SuoError("Unsupport metadata datype in JSON message");
			}
		}

		if (dict.contains("data")) {

			/* Parse ASCII hexadecimal string to bytes */
			string hex_string = dict["data"];
			if (hex_string.size() % 2 != 0)
				throw SuoError("JSON data field has odd number of characters!");

			size_t frame_len = hex_string.size() / 2;
			frame.data.resize(frame_len);
			for (size_t i = 0; i < frame_len; i++)
				frame.data[i] = stoul(hex_string.substr(2 * i, 2), nullptr, 16);
		}
		else {
			frame.flags |= Frame::Flags::control_frame;
		}

		return frame;
	}
	catch (const json::exception& e) { //basic_json::type_error 
		throw SuoError("Failed to parse JSON dictionary. %s", e.what());
	}
}


string Frame::serialize_to_json() const {

	json dict = json::object();
	dict["id"] = id;

	/* Format metadata to a JSON dictionary */
	json meta_dict = json::object();
	for (auto metadata : metadata) {
		std::visit([&](auto const& a) {
			meta_dict[metadata.first] = a;
		}, metadata.second);
	}
	dict["metadata"] = meta_dict;


	if (1) // flags & Frame::Flags::has_timestamp)
		dict["timestamp"] = timestamp;

	/* Format binary data to hexadecimal string */
	stringstream hexa_stream;
	hexa_stream << setfill('0') << hex;
	for (size_t i = 0; i < data.size(); i++)
		hexa_stream << setw(2) << (int)data[i];
	dict["data"] = hexa_stream.str();

	return dict.dump();
}


