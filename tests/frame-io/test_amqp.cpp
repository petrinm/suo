
#include <iostream>
#include <unistd.h>

#include <cppunit/TestFixture.h>
#include <cppunit/TestAssert.h>
#include <cppunit/TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include "suo.hpp"
#include "frame-io/amqp_interface.hpp"


using namespace std;
using namespace suo;


class AMQPTest: public CppUnit::TestFixture
{
private:
	Timestamp now;

public:

	/* Sleep given amount of milliseconds */
	void sleep(unsigned int ms) {
		usleep((ms) * 1000);
		now += (ms) * 1000;
	}

	void setUp() {
		srand(time(nullptr));
		now = time(nullptr) % 0xFFFFFF;
	}

	void runTest() {

		/*
		 * Create AMQP interface
		 */
		AMQPInterface::Config conf;
        conf.amqp_url = "amqp://guest:guest@localhost/";
        conf.exchange = "foresail1";
        conf.rx_routing_key = "uplink";
        conf.tx_routing_key = "downlink";

		AMQPInterface amqp(conf);

        Frame in_frame(256);

        for (int i = 0; i < 1000; i++) {
		    sleep(10);
            amqp.tick(now);

            if (i == 100) {

                Frame out_frame(256);
                out_frame.timestamp = now;
                out_frame.setMetadata("cfo", 12.345);
                out_frame.setMetadata("rssi", -123.4);
                out_frame.data.resize(64);
                for (int i = 0; i < 64; i++)
                    out_frame.data[i] = rand() % 256;

                cout << "Transmitting:" << endl;
                cout << out_frame << endl;
                amqp.sinkFrame(out_frame, now);
            }

            // Try to source new frames
            amqp.sourceFrame(in_frame, now);
            if (in_frame.empty() == false) {

                cout << "Received:" << endl;
                cout << in_frame(Frame::PrintAltColor) << endl;

                in_frame.clear();
            }
        }

	}

};


int main(int argc, char** argv)
{
	CppUnit::TextUi::TestRunner runner;
	runner.addTest(new CppUnit::TestCaller<AMQPTest>("AMQPTest", &AMQPTest::runTest));
	runner.run();
	return 0;
}

