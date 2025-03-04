#include "AudioFileSourceSDFAT.h"

AudioFileSourceSDFAT::AudioFileSourceSDFAT():_semaphore(nullptr),_useSemaphore(false){}

AudioFileSourceSDFAT::AudioFileSourceSDFAT(SemaphoreHandle_t semaphore):_semaphore(semaphore),_useSemaphore(true){}

AudioFileSourceSDFAT::AudioFileSourceSDFAT(const char *filename):_semaphore(nullptr),_useSemaphore(false){
	open(filename);
}

AudioFileSourceSDFAT::AudioFileSourceSDFAT(const char *filename,SemaphoreHandle_t semaphore): _semaphore(semaphore),_useSemaphore(true){
	open(filename);
}

bool AudioFileSourceSDFAT::open(const char *filename){
	bool result=false;
	if(_useSemaphore&&_semaphore){
		if(xSemaphoreTake(_semaphore,portMAX_DELAY)==pdTRUE){
			result=f.open(filename,O_RDONLY);
			xSemaphoreGive(_semaphore);
		}
	}else{
		result=f.open(filename,O_RDONLY);
	}
	return result;
}

AudioFileSourceSDFAT::~AudioFileSourceSDFAT(){
	if(_useSemaphore&&_semaphore){
		if(xSemaphoreTake(_semaphore,portMAX_DELAY)==pdTRUE){
			if(f.isOpen()) f.close();
			xSemaphoreGive(_semaphore);
		}
	}else{
		if(f.isOpen()) f.close();
	}
}

uint32_t AudioFileSourceSDFAT::read(void *data,uint32_t len){
	uint32_t result=0;
	if(_useSemaphore&&_semaphore){
		if(xSemaphoreTake(_semaphore,portMAX_DELAY)==pdTRUE){
			result=(f.available())?f.read(data,len):0;
			xSemaphoreGive(_semaphore);
		}
	}else{
		result=(f.available())?f.read(data,len):0;
	}
	return result;
}

bool AudioFileSourceSDFAT::seek(int32_t pos,int dir){
	bool result=false;
	if(_useSemaphore&&_semaphore){
		if(xSemaphoreTake(_semaphore,portMAX_DELAY)==pdTRUE){
			if(!f.isOpen()&&!f.available()){
				xSemaphoreGive(_semaphore);
				return false;
			}
			if(dir==SEEK_SET) result=f.seekSet(pos);
			else if(dir==SEEK_CUR) result=f.seekCur(pos);
			else if(dir==SEEK_END) result=f.seekEnd(pos);
			xSemaphoreGive(_semaphore);
		}
	}else{
		if(!f.isOpen()&&!f.available()) return false;
		if(dir==SEEK_SET) result=f.seekSet(pos);
		else if(dir==SEEK_CUR) result=f.seekCur(pos);
		else if(dir==SEEK_END) result=f.seekEnd(pos);
	}
	return result;
}

bool AudioFileSourceSDFAT::close(){
	bool result=false;
	if(_useSemaphore&&_semaphore){
		if(xSemaphoreTake(_semaphore,portMAX_DELAY)==pdTRUE){
			f.close();
			result=true;
			xSemaphoreGive(_semaphore);
		}
	}else{
		f.close();
		result=true;
	}
	return result;
}

bool AudioFileSourceSDFAT::isOpen(){
	bool result=false;
	if(_useSemaphore&&_semaphore){
		if(xSemaphoreTake(_semaphore, portMAX_DELAY)==pdTRUE){
			result=f.isOpen();
			xSemaphoreGive(_semaphore);
		}
	}else{
		result=f.isOpen();
	}
	return result;
}

uint32_t AudioFileSourceSDFAT::getSize(){
	uint32_t result=0;
	if(_useSemaphore&&_semaphore){
		if(xSemaphoreTake(_semaphore,portMAX_DELAY)==pdTRUE){
			result=f.isOpen()?f.fileSize():0;
			xSemaphoreGive(_semaphore);
		}
	}else{
		result=f.isOpen()?f.fileSize():0;
	}
	return result;
}

uint32_t AudioFileSourceSDFAT::getPos(){
	uint32_t result=0;
	if(_useSemaphore&&_semaphore){
		if(xSemaphoreTake(_semaphore,portMAX_DELAY)==pdTRUE){
			result=f.isOpen()?f.curPosition():0;
			xSemaphoreGive(_semaphore);
		}
	}else{
		result=f.isOpen()?f.curPosition():0;
	}
	return result;
}
