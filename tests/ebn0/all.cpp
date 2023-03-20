

#define COMBINED_EBNO_TEST

#include "bpsk_golay.cpp"
#include "fsk_matched_golay.cpp"
#include "fsk_matched_hdlc.cpp"
#include "gmsk_cont_golay.cpp"
#include "gmsk_cont_hdlc.cpp"


int main(int argc, char** argv)
{
	bpsk_golay_test();
	fsk_matched_golay_test();
	fsk_matched_hdlc_test();
	gmsk_cont_golay_test();
	gmsk_cont_hdlc_test();
	return 0;
}
