
#include <iostream>
#include <unistd.h>

#include <cppunit/TestFixture.h>
#include <cppunit/TestAssert.h>
#include <cppunit/TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include "suo.hpp"
#include "frame-io/zmq_interface.hpp"


using namespace std;
using namespace suo;


class ZMQTest : public CppUnit::TestFixture
{
private:
	Timestamp now;

public:

	/* Sleep given amount of milliseconds */
	void sleep(unsigned int ms) {
		//usleep((ms) * 1000);
		now += (ms) * 1000;
	}

	void setUp() { 
		srand(time(nullptr));
		now = time(nullptr) % 0xFFFFFF;
	}

	void tearDown() { }

	void runTest() {

		/*
		 * Create ZMQ TX input
		 */
		ZMQInput::Config in_conf;
		in_conf.address = "tcp://127.0.0.1:43301";
		in_conf.address_tick = "tcp://127.0.0.1:43303";
		//in_conf.flags = 15; // ZMQIO_BIND | ZMQIO_METADATA | ZMQIO_THREAD | ZMQIO_BIND_TICK;

		ZMQInput zmq_input(in_conf);

		sleep(1000);

		/*
		 * Create ZMQ RX output
		 */
		ZMQOutput::Config out_conf;
		out_conf.address = "tcp://127.0.0.1:43301";
		out_conf.address_tick = "tcp://127.0.0.1:43303";
		//out_conf.flags = 15; // 6 = ZMQIO_METADATA | ZMQIO_THREAD

		ZMQOutput zmq_output(out_conf);

		sleep(200);

		/*
		* Create test frame
		*/
		Frame out_frame(256);
		out_frame.timestamp = now;
		out_frame.setMetadata("cfo", 12.345);
		out_frame.setMetadata("rssi", -123.4);
		out_frame.data.resize(64);
		for (int i = 0; i < 64; i++)
			out_frame.data[i] = rand() % 256;


		cout << "Sent:" << endl << out_frame;
		//suo_frame_print(out_frame, SUO_PRINT_DATA | SUO_PRINT_METADATA | SUO_PRINT_COLOR);

		sleep(600);

		/*
		* Send the frame twice
		*/
		zmq_output.sinkFrame(out_frame, now);
		now += 1000;
		zmq_output.sinkFrame(out_frame, now);

		sleep(200);

		/*
		* Receive the frame
		*/
		Frame in_frame(256);
		CPPUNIT_ASSERT(zmq_input.sourceFrame(in_frame, now) == 1);

		cout << "Received:" << endl << in_frame;
		//suo_frame_print(in_frame, SUO_PRINT_DATA | SUO_PRINT_METADATA);

		CPPUNIT_ASSERT(in_frame->data_len == out_frame->data_len);
		CPPUNIT_ASSERT(memcmp(in_frame->data, out_frame->data, in_frame->data_len) == 0);
		in_frame.clear();

		// Receive second
		CPPUNIT_ASSERT(zmq_input.sourceFframe(in_frame, now) == 1);

		cout << "Received:" << endl << in_frame;
		//suo_frame_print(in_frame, SUO_PRINT_DATA | SUO_PRINT_METADATA);
		CPPUNIT_ASSERT(in_frame->data_len == out_frame->data_len);
		CPPUNIT_ASSERT(memcmp(in_frame->data, out_frame->data, in_frame->data_len) == 0);



		// Third time fails
		CPPUNIT_ASSERT(zmq_input.source_frame(in_frame, time) == 0);

		/*
		 * Test ticks
		 */
		{

		}


	}

};


int main(int argc, char** argv)
{
	CppUnit::TextUi::TestRunner runner;
	runner.addTest(new CppUnit::TestCaller<ZMQTest>("ZMQTest", &ZMQTest::runTest));
	runner.run();
	return 0;
}
