#ifndef PACKETRECORD_H
#define PACKETRECORD_H

#include <iostream>

struct PacketRecord {
private:
	char *src_nodeaddr;
	char *dst_nodeaddr;
	char action;
	int uid;	
public:
	PacketRecord() {
		src_nodeaddr = new char;
		*src_nodeaddr = '\0';
		dst_nodeaddr = new char;
		*dst_nodeaddr = '\0';
		action = '\0';
		uid = 0;
	}

	PacketRecord(const PacketRecord& key) {
		int fromLen = strlen(key.src_nodeaddr)+1;
		src_nodeaddr = new char[fromLen];
		strcpy(src_nodeaddr, key.src_nodeaddr);

		int toLen = strlen(key.dst_nodeaddr)+1;
		dst_nodeaddr = new char[toLen];
		strcpy(dst_nodeaddr, key.dst_nodeaddr);

		action = key.action;
		uid = key.uid; 
	}

	~PacketRecord(){
		delete src_nodeaddr;
		delete dst_nodeaddr;
	}

	PacketRecord& operator=(const PacketRecord& key) {
		if (*this == key)
		{
			return *this;
		}

		delete src_nodeaddr;
		delete dst_nodeaddr;

		int fromLen = strlen(key.src_nodeaddr)+1;
		src_nodeaddr = new char[fromLen];
		strcpy(src_nodeaddr, key.src_nodeaddr);

		int toLen = strlen(key.dst_nodeaddr)+1;
		dst_nodeaddr = new char[toLen];
		strcpy(dst_nodeaddr, key.dst_nodeaddr);

		action = key.action;
		uid = key.uid;

		return *this;
	}

	friend bool operator==(const PacketRecord& a, const PacketRecord& b) {
		if( a.uid == b.uid 
			&& a.action == b.action 
			&& strcmp(a.src_nodeaddr, b.dst_nodeaddr) == 0 
			&& strcmp(a.dst_nodeaddr, b.dst_nodeaddr) == 0)
			return true;
		return false;
	}
	
	friend bool operator<(const PacketRecord& a, const PacketRecord& b) {
		int fromCmp = strcmp(a.src_nodeaddr, b.src_nodeaddr);
		int toCmp = strcmp(a.dst_nodeaddr, b.dst_nodeaddr);

		if (a.uid != b.uid) {
			return a.uid < b.uid ? true:false;
		}
		else if (a.action != b.action) {
			return a.action < b.action ? true:false;
		}
		else if (fromCmp != 0) {
			return fromCmp < 0 ? true:false;
		}
		else {
			return toCmp < 0 ? true:false;
		}
	}
	
	friend bool operator>(const PacketRecord& a, const PacketRecord& b) {
		int fromCmp = strcmp(a.src_nodeaddr, b.src_nodeaddr);
		int toCmp = strcmp(a.dst_nodeaddr, b.dst_nodeaddr);

		if (a.uid != b.uid) {
			return a.uid > b.uid ? true:false;
		}
		else if (a.action != b.action) {
			return a.action > b.action ? true:false;
		}
		else if (fromCmp != 0) {
			return fromCmp > 0 ? true:false;
		}
		else  {
			return toCmp > 0 ? true:false;
		}
	}

	void src_nodeaddrSet(const char* str) {
		int	srcLen = strlen(str) + 1;
		src_nodeaddr = new char[srcLen];
		strncpy(src_nodeaddr, str, srcLen);
	}

	void dst_nodeaddrSet(const char* str) {
		int dstLen = strlen(str) + 1;
		dst_nodeaddr = new char[dstLen];
		strncpy(dst_nodeaddr, str, dstLen);
	}

	void actionSet(const char flag) {
		action = flag;
	}

	void uidSet(const int val) {
		uid = val;
	}
};

#endif