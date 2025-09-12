/* Author: Romain "Artefact2" Dalmaso <artefact2@gmail.com> */
/* Contributor: Dan Spencer <dan@atomicpotato.net> */

/* This program is free software. It comes without any warranty, to the
 * extent permitted by applicable law. You can redistribute it and/or
 * modify it under the terms of the Do What The Fuck You Want To Public
 * License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/wtfpl/COPYING for more details. */

#include "xm_internal.h"

/* .xm files are little-endian. (XXX: Are they really?) */

extern bool file_seek_set(size_t offset);
extern bool file_seek_cur(size_t offset);
extern uint8_t xm_read_u8();
extern void xm_read_memcpy(void* ptr, size_t length);

#define FILE_OFFSET(offset) file_seek_set(offset)
#define FILE_SKIP(bytes) file_seek_cur(bytes)
#define READ_U8() xm_read_u8()
#define READ_U16() ((uint16_t)READ_U8() | ((uint16_t)READ_U8() << 8))
#define READ_U32() ((uint32_t)READ_U16() | ((uint32_t)READ_U16() << 16))
#define READ_MEMCPY(ptr, length) xm_read_memcpy(ptr, length)

static inline void memcpy_pad(void* dst, size_t dst_len, const void* src, size_t src_len, size_t offset) {
	uint8_t* dst_c = dst;
	const uint8_t* src_c = src;

	/* how many bytes can be copied without overrunning `src` */
	size_t copy_bytes = (src_len >= offset) ? (src_len - offset) : 0;
	copy_bytes = copy_bytes > dst_len ? dst_len : copy_bytes;

	memcpy(dst_c, src_c + offset, copy_bytes);
	/* padded bytes */
	memset(dst_c + copy_bytes, 0, dst_len - copy_bytes);
}

int xm_check_sanity_preload(const char* module, size_t module_length) {
	if(module_length < 60) {
		return 4;
	}

	if(memcmp("Extended Module: ", module, 17) != 0) {
		return 1;
	}

	if(module[37] != 0x1A) {
		return 2;
	}

	if(module[59] != 0x01 || module[58] != 0x04) {
		/* Not XM 1.04 */
		return 3;
	}

	return 0;
}

int xm_check_sanity_postload(xm_context_t* ctx) {
	/* @todo: plenty of stuff to do here… */

	/* Check the POT */
	for(uint8_t i = 0; i < ctx->module.length; ++i) {
		if(ctx->module.pattern_table[i] >= ctx->module.num_patterns) {
			if(i+1 == ctx->module.length && ctx->module.length > 1) {
				/* Cheap fix */
				--ctx->module.length;
				DEBUG("trimming invalid POT at pos %X", i);
			} else {
				DEBUG("module has invalid POT, pos %X references nonexistent pattern %X",
				      i,
				      ctx->module.pattern_table[i]);
				return 1;
			}
		}
	}

	return 0;
}

size_t xm_get_memory_needed_for_context(const char* moddata,size_t moddata_length){
	size_t memory_needed=0;
	size_t offset=60; /* Skip the first header */
	uint16_t num_channels;
	uint16_t num_patterns;
	uint16_t num_instruments;
	/* Read the module header */
	FILE_OFFSET(offset);
	uint32_t header_size=READ_U32();
	uint16_t module_length=READ_U16();
	memory_needed+=MAX_NUM_ROWS*module_length*sizeof(uint8_t);
	READ_U16(); // skip restart_position
	num_channels=READ_U16();
	num_patterns=READ_U16();
	memory_needed+=num_patterns*sizeof(xm_pattern_t);
	num_instruments=READ_U16();
	memory_needed+=num_instruments*sizeof(xm_instrument_t);
	/* Header size */
	offset+=header_size;
	/* Read pattern headers */
	for(uint16_t i=0;i<num_patterns;i++){
		FILE_OFFSET(offset);
		uint32_t pattern_header_length=READ_U32();
		READ_U8(); // skip pattern packing type
		uint16_t num_rows=READ_U16();
		memory_needed+=num_rows*num_channels*sizeof(xm_pattern_slot_t);
		uint16_t packed_size=READ_U16();
		/* Pattern header length + packed pattern data size */
		offset+=pattern_header_length+packed_size;
	}
	/* Read instrument headers */
	for(uint16_t i=0;i<num_instruments;i++){
		FILE_OFFSET(offset);
		uint32_t instrument_header_size=READ_U32();
		// Skip to num_samples field
		FILE_OFFSET(offset+27);
		uint16_t num_samples=READ_U16();
		memory_needed+=num_samples*sizeof(xm_sample_t);
		uint32_t sample_header_size=0;
		uint32_t sample_size_aggregate=0;
		if(num_samples>0){
			FILE_OFFSET(offset+29);
			sample_header_size=READ_U32();
		}
		/* Instrument header size */
		offset+=instrument_header_size;
		for(uint16_t j=0;j<num_samples;j++){
			FILE_OFFSET(offset);
			uint32_t sample_size=READ_U32();
			sample_size_aggregate+=sample_size;
			memory_needed+=sample_size;
			offset+=sample_header_size;
		}
		offset+=sample_size_aggregate;
	}
	memory_needed+=num_channels*sizeof(xm_channel_context_t);
	memory_needed+=sizeof(xm_context_t);
	return memory_needed;
}

// result x10 faster than original!
char* xm_load_module(xm_context_t* ctx,const char* moddata,size_t moddata_length,char* mempool){
	size_t offset=0;
	xm_module_t* mod=&(ctx->module);
	uint8_t buffer[512];
#if XM_STRINGS
	FILE_OFFSET(17);
	READ_MEMCPY(mod->name,MODULE_NAME_LENGTH);
	FILE_OFFSET(38);
	READ_MEMCPY(mod->trackername,TRACKER_NAME_LENGTH);
#endif
	offset+=60;
	FILE_OFFSET(offset);
	READ_MEMCPY(buffer,276);
	uint32_t header_size=*(uint32_t*)buffer;
	mod->length=*(uint16_t*)(buffer+4);
	mod->restart_position=*(uint16_t*)(buffer+6);
	mod->num_channels=*(uint16_t*)(buffer+8);
	mod->num_patterns=*(uint16_t*)(buffer+10);
	mod->num_instruments=*(uint16_t*)(buffer+12);
	mod->patterns=(xm_pattern_t*)mempool;
	mempool+=mod->num_patterns*sizeof(xm_pattern_t);
	mod->instruments=(xm_instrument_t*)mempool;
	mempool+=mod->num_instruments*sizeof(xm_instrument_t);
	uint16_t flags=*(uint16_t*)(buffer+14);
	mod->frequency_type=(flags&1)?XM_LINEAR_FREQUENCIES:XM_AMIGA_FREQUENCIES;
	ctx->tempo=*(uint16_t*)(buffer+16);
	ctx->bpm=*(uint16_t*)(buffer+18);
	memcpy(mod->pattern_table,buffer+20,PATTERN_ORDER_TABLE_LENGTH);
	offset+=header_size;
	for(uint16_t i=0;i<mod->num_patterns;i++){
		FILE_OFFSET(offset);
		READ_MEMCPY(buffer,9);
		uint32_t pattern_header_length=*(uint32_t*)buffer;
		xm_pattern_t* pat=mod->patterns+i;
		pat->num_rows=*(uint16_t*)(buffer+5);
		uint16_t packed_patterndata_size=*(uint16_t*)(buffer+7);
		pat->slots=(xm_pattern_slot_t*)mempool;
		mempool+=mod->num_channels*pat->num_rows*sizeof(xm_pattern_slot_t);
		offset+=pattern_header_length;
		if(packed_patterndata_size==0){
			memset(pat->slots,0,sizeof(xm_pattern_slot_t)*pat->num_rows*mod->num_channels);
		}else{
			if(packed_patterndata_size<=512){
				FILE_OFFSET(offset);
				READ_MEMCPY(buffer,packed_patterndata_size);
				uint8_t* data_ptr=buffer;
				for(uint16_t j=0,k=0;j<packed_patterndata_size;k++){
					uint8_t note=*data_ptr++;
					xm_pattern_slot_t* slot=pat->slots+k;
					j++;
					if(note&128){
						slot->note=(note&1)?*data_ptr++:0;
						if(note&1)j++;
						slot->instrument=(note&2)?*data_ptr++:0;
						if(note&2)j++;
						slot->volume_column=(note&4)?*data_ptr++:0;
						if(note&4)j++;
						slot->effect_type=(note&8)?*data_ptr++:0;
						if(note&8)j++;
						slot->effect_param=(note&16)?*data_ptr++:0;
						if(note&16)j++;
					}else{
						slot->note=note;
						slot->instrument=*data_ptr++;
						slot->volume_column=*data_ptr++;
						slot->effect_type=*data_ptr++;
						slot->effect_param=*data_ptr++;
						j+=4;
					}
				}
			}else{
				FILE_OFFSET(offset);
				for(uint16_t j=0,k=0;j<packed_patterndata_size;k++){
					uint8_t note=READ_U8();
					xm_pattern_slot_t* slot=pat->slots+k;
					j++;
					if(note&128){
						slot->note=(note&1)?READ_U8():0;
						if(note&1)j++;
						slot->instrument=(note&2)?READ_U8():0;
						if(note&2)j++;
						slot->volume_column=(note&4)?READ_U8():0;
						if(note&4)j++;
						slot->effect_type=(note&8)?READ_U8():0;
						if(note&8)j++;
						slot->effect_param=(note&16)?READ_U8():0;
						if(note&16)j++;
					}else{
						slot->note=note;
						slot->instrument=READ_U8();
						slot->volume_column=READ_U8();
						slot->effect_type=READ_U8();
						slot->effect_param=READ_U8();
						j+=4;
					}
				}
			}
		}
		offset+=packed_patterndata_size;
	}
	for(uint16_t i=0;i<ctx->module.num_instruments;i++){
		uint32_t sample_header_size=0;
		xm_instrument_t* instr=mod->instruments+i;
		FILE_OFFSET(offset);
		READ_MEMCPY(buffer,243);
		uint32_t instrument_header_size=*(uint32_t*)buffer;
#if XM_STRINGS
		memcpy(instr->name,buffer+4,INSTRUMENT_NAME_LENGTH);
#endif
		instr->num_samples=*(uint16_t*)(buffer+27);
		if(instr->num_samples>0){
			sample_header_size=*(uint32_t*)(buffer+29);
			memcpy(instr->sample_of_notes,buffer+33,NUM_NOTES);
			uint8_t* env_ptr=buffer+129;
			for(uint8_t j=0;j<12;j++){
				instr->volume_envelope.points[j].frame=*(uint16_t*)(env_ptr+j*4);
				instr->volume_envelope.points[j].value=*(uint16_t*)(env_ptr+j*4+2);
			}
			env_ptr+=48;
			for(uint8_t j=0;j<12;j++){
				instr->panning_envelope.points[j].frame=*(uint16_t*)(env_ptr+j*4);
				instr->panning_envelope.points[j].value=*(uint16_t*)(env_ptr+j*4+2);
			}
			env_ptr+=48;
			instr->volume_envelope.num_points=*env_ptr++;
			instr->panning_envelope.num_points=*env_ptr++;
			instr->volume_envelope.sustain_point=*env_ptr++;
			instr->volume_envelope.loop_start_point=*env_ptr++;
			instr->volume_envelope.loop_end_point=*env_ptr++;
			instr->panning_envelope.sustain_point=*env_ptr++;
			instr->panning_envelope.loop_start_point=*env_ptr++;
			instr->panning_envelope.loop_end_point=*env_ptr++;
			uint8_t flags=*env_ptr++;
			instr->volume_envelope.enabled=flags&1;
			instr->volume_envelope.sustain_enabled=flags&2;
			instr->volume_envelope.loop_enabled=flags&4;
			flags=*env_ptr++;
			instr->panning_envelope.enabled=flags&1;
			instr->panning_envelope.sustain_enabled=flags&2;
			instr->panning_envelope.loop_enabled=flags&4;
			instr->vibrato_type=*env_ptr++;
			if(instr->vibrato_type==2){
				instr->vibrato_type=1;
			}else if(instr->vibrato_type==1){
				instr->vibrato_type=2;
			}
			instr->vibrato_sweep=*env_ptr++;
			instr->vibrato_depth=*env_ptr++;
			instr->vibrato_rate=*env_ptr++;
			instr->volume_fadeout=*(uint16_t*)env_ptr;
			instr->samples=(xm_sample_t*)mempool;
			mempool+=instr->num_samples*sizeof(xm_sample_t);
		}else{
			instr->samples=NULL;
		}
		offset+=instrument_header_size;
		for(uint16_t j=0;j<instr->num_samples;j++){
			FILE_OFFSET(offset);
			READ_MEMCPY(buffer,40);
			xm_sample_t* sample=instr->samples+j;
			sample->length=*(uint32_t*)buffer;
			sample->loop_start=*(uint32_t*)(buffer+4);
			sample->loop_length=*(uint32_t*)(buffer+8);
			sample->loop_end=sample->loop_start+sample->loop_length;
			sample->volume=(float)buffer[12]/(float)0x40;
			sample->finetune=(int8_t)buffer[13];
			uint8_t flags=buffer[14];
			if((flags&3)==0){
				sample->loop_type=XM_NO_LOOP;
			}else if((flags&3)==1){
				sample->loop_type=XM_FORWARD_LOOP;
			}else{
				sample->loop_type=XM_PING_PONG_LOOP;
			}
			sample->bits=(flags&16)?16:8;
			sample->panning=(float)buffer[15]/(float)0xFF;
			sample->relative_note=(int8_t)buffer[16];
#if XM_STRINGS
			memcpy(sample->name,buffer+18,SAMPLE_NAME_LENGTH);
#endif
			sample->data8=(int8_t*)mempool;
			mempool+=sample->length;
			if(sample->bits==16){
				sample->loop_start>>=1;
				sample->loop_length>>=1;
				sample->loop_end>>=1;
				sample->length>>=1;
			}
			offset+=sample_header_size;
		}
		for(uint16_t j=0;j<instr->num_samples;j++){
			xm_sample_t* sample=instr->samples+j;
			uint32_t length=sample->length;
			if(sample->bits==16){
				FILE_OFFSET(offset);
				size_t bytes_to_read=length*2;
				size_t bytes_read=0;
				int16_t v=0;
				while(bytes_read<bytes_to_read){
					size_t chunk_size=(bytes_to_read-bytes_read>512)?512:(bytes_to_read-bytes_read);
					READ_MEMCPY(buffer,chunk_size);
					for(size_t k=0;k<chunk_size;k+=2){
						v=v+(int16_t)(buffer[k]|(buffer[k+1]<<8));
						sample->data16[bytes_read/2+k/2]=v;
					}
					bytes_read+=chunk_size;
				}
				offset+=length<<1;
			}else{
				FILE_OFFSET(offset);
				size_t bytes_read=0;
				int8_t v=0;
				while(bytes_read<length){
					size_t chunk_size=(length-bytes_read>512)?512:(length-bytes_read);
					READ_MEMCPY(buffer,chunk_size);
					for(size_t k=0;k<chunk_size;k++){
						v=v+(int8_t)buffer[k];
						sample->data8[bytes_read+k]=v;
					}
					bytes_read+=chunk_size;
				}
				offset+=length;
			}
		}
	}
	return mempool;
}
