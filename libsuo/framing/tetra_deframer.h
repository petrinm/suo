/*
 * TETRA deframer
 */
#ifndef __SUO_FRAMING_TETRA_FRAMER_H__
#define __SUO_FRAMING_TETRA_FRAMER_H__


struct tetra_deframer_conf {
	uint64_t syncword1, syncword2, syncword3;
	unsigned synclen1, synclen2, synclen3;
	unsigned syncpos;
	unsigned framelen;
};



#endif /* __SUO_FRAMING_TETRA_FRAMER_H__ */
