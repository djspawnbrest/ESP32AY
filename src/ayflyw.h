void ay_resetay(AYSongInfo* ay,int chip){
  if(!chip) ay_reset();
}

void ay_writeay(AYSongInfo* ay,uint8_t reg,uint8_t val){
  ay_write_remap(0,reg,val);
}

void ay_writeay(AYSongInfo* ay,uint8_t reg,uint8_t val,uint8_t chip){
  if(!chip){
    ay_write_remap(0,reg,val);
  }else{
    ay_write_remap(1,reg,val);
  }
}

unsigned char ay_readay(AYSongInfo* ay,uint8_t reg,uint8_t chip){
  if(reg<16){
    if(!chip) return ay_reg_1[reg]; else return ay_reg_2[reg];
  }
  return 0;
}

uint16_t ay_sys_getword(unsigned char* src){
  return src[0]+(src[1]<<8);
}

void ay_sys_writeword(unsigned char* dst,uint16_t val){
  dst[0]=val&0xff;
  dst[1]=val>>8;
}

void ay_sys_getstr(char* dst,unsigned int dst_size,unsigned char* src,unsigned int src_maxlen){
  memset(dst,0,dst_size);
  for(int i=0;i<src_maxlen;i++){
    char c=src[i];
    if(!c) break;
    dst[i]=c;
  }
  int i=strlen(dst)-1;
  while (i>=0){
    if(dst[i]!=' ') break;
    dst[i--]=0;
  }
}