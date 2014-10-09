// ITU-T H.222.0(06/2012)
// Information technology �C Generic coding of moving pictures and associated audio information: Systems
// 2.5.4 Program stream map(p79)

#include "mpeg-ps-proto.h"
#include "mpeg-pes-proto.h"
#include "mpeg-util.h"
#include "crc32.h"
#include <assert.h>

size_t psm_read(const uint8_t* data, size_t bytes, psm_t* psm)
{
	int i, j, k, len;
	uint32_t crc;
	uint8_t current_next_indicator;
	uint8_t single_extension_stream_flag;
	uint16_t program_stream_map_length;
	uint16_t program_stream_info_length;
	uint16_t element_stream_map_length;
	uint16_t element_stream_info_length;

	// Table 2-41 �C Program stream map(p79)
	assert(0x00==data[0] && 0x00==data[1] && 0x01==data[2] && 0xBC==data[3]);
	program_stream_map_length = (data[4] << 8) | data[5];

	//assert((0x20 & data[6]) == 0x00); // 'xx0xxxxx'
	current_next_indicator = (data[6] >> 7) & 0x01;
	single_extension_stream_flag = (data[6] >> 6) & 0x01;
	psm->ver = data[6] & 0x1F;
	//assert(data[7] == 0x01); // '00000001'

	// program stream descriptor
	program_stream_info_length = (data[8] << 8) | data[9];

	// TODO: parse descriptor

	// program element stream
	i = 10 + program_stream_info_length;
	element_stream_map_length = (data[i] << 8) | data[i+1];

	j = i + 2;
	psm->stream_count = 0;
	while(j < i+2+element_stream_map_length)
	{
		psm->streams[psm->stream_count].avtype = data[j];
		psm->streams[psm->stream_count].pesid = data[j+1];
		element_stream_info_length = (data[j+2] << 8) | data[j+3];

		k = j + 4;
		if(0xFD == psm->streams[psm->stream_count].pesid && 0 == single_extension_stream_flag)
		{
			uint8_t pseudo_descriptor_tag = data[k];
			uint8_t pseudo_descriptor_length = data[k+1];
			uint8_t element_stream_id_extension = data[k+2] & 0x7F;
			assert((0x80 & data[k+2]) == 0x80); // '1xxxxxxx'
			k += 3;
		}

		while(k < j + 4 + element_stream_info_length)
		{
			// descriptor()
			k += mpeg_elment_descriptor(data+k, bytes-k);
		}

		++psm->stream_count;
		assert(k - j - 4 == element_stream_info_length);
		j += 4 + element_stream_info_length;
	}

//	assert(j+4 == program_stream_map_length+6);
//	assert(0 == crc32(-1, data, program_stream_map_length+6));
	return program_stream_map_length+6;
}

size_t psm_write(const psm_t *psm, uint8_t *data)
{
	// Table 2-41 �C Program stream map(p79)

	size_t i,j;
	unsigned int crc;

	le_write_uint32(data, 0x00000100);
	data[3] = PES_SID_PSM;

	// program_stream_map_length 16-bits
	//le_write_uint16(data+4, 6+4*psm->stream_count+4);

	// current_next_indicator '1'
	// single_extension_stream_flag '1'
	// reserved '0'
	// program_stream_map_version 'xxxxx'
	data[6] = 0xc0 | (psm->ver & 0x1F);

	// reserved '0000000'
	// marker_bit '1'
	data[7] = 0x01;

	// program_stream_info_length 16-bits
	le_write_uint16(data+8, 0); // program_stream_info_length = 0

	// elementary_stream_map_length 16-bits
	//le_write_uint16(data+10, psm->stream_count*4);

	j = 12;
	for(i = 0; i < psm->stream_count; i++)
	{
		assert(PES_SID_EXTEND != psm->streams[i].pesid);

		// stream_type:8
		data[j++] = psm->streams[i].avtype;
		// elementary_stream_id:8
		data[j++] = psm->streams[i].pesid;
		// elementary_stream_info_length:16
		le_write_uint16(data+j, psm->streams[i].esinfo_len);
		// descriptor()
		memcpy(data+j+2, psm->streams[i].esinfo, psm->streams[i].esinfo_len);

		j += 2 + psm->streams[i].esinfo_len;
	}

	// elementary_stream_map_length 16-bits
	le_write_uint16(data+10, j-12+4);
	// program_stream_map_length:16
	le_write_uint16(data+4, j-6+4); // 4-bytes crc32

	// crc32
	crc = crc32(0xffffffff, data, j);
	crc = 0;
	data[j+3] = (uint8_t)((crc >> 24) & 0xFF);
	data[j+2] = (uint8_t)((crc >> 16) & 0xFF);
	data[j+1] = (uint8_t)((crc >> 8) & 0xFF);
	data[j+0] = (uint8_t)(crc & 0xFF);

	return j+4;
}