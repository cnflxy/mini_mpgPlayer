#ifndef _MMP_TAG_H_
#define _MMP_TAG_H_ 1

#include "bs.h"
#include "frame.h"

void decode_id3v1(struct bs* const bstream);
int decode_id3v2(struct bs* const bstream, uint32_t* const size);
int get_vbr_tag(const struct bs* const bstream, const struct mpeg_frame* frame);

#endif // !_MMP_TAG_H_
