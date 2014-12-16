/*
 * Node.h
 *
 *  Created on: Dec 8, 2014
 *      Author: pat2
 */

#ifndef LIB_PARALLELXML_LOCKFREEXMLNODE_HPP_
#define LIB_PARALLELXML_LOCKFREEXMLNODE_HPP_

#include <string>
#include <list>
#include "Outstream.h"

#include <lib/support/IteratorStack.hpp>

using std::string;

struct Attribute {
	string name;
	string value;
	Attribute(const string& _name, const string& _value):
		name(_name), value(_value) {}
	Attribute(string& _name, string& _value):
			name(_name), value(_value) {}
};
typedef std::list<Attribute> AttributeList;
#if __cilk
#include <cilk/cilk.h>
//#include <cilk/reducer_list.h>
//typedef cilk::reducer<cilk::op_list_append<Attribute> > AttributeList;
#else
#include <vector>
//typedef std::list<Attribute>* AttributeList;
#define cilk_spawn
#define cilk_sync
#endif


enum AttributeStatus {
	OPEN, FINALIZED, WRITING, WRITTEN
};

class LockFreeXMLNode {
public:
	LockFreeXMLNode(const string& tagName, std::ostream* outstream);
	LockFreeXMLNode(LockFreeXMLNode* parent, const string& tagName);

	LockFreeXMLNode* createNode(const string& tagName);

	void AddAttribute(Attribute);
	// Locks attributes, no further changes allowed. They may be written out
	void FinalizeAttributes();
	void AddChild(LockFreeXMLNode* newChild);
	void TransferAllMyChildrenTo(LockFreeXMLNode* newParent);
	virtual ~LockFreeXMLNode();

	LockFreeXMLNode* Parent() const {return parent;}
	LockFreeXMLNode* FirstChild() const {return firstChild;}
	LockFreeXMLNode* LastChild() const {return lastChild;}
	LockFreeXMLNode* NextSibling() const {return nextSibling;}
	LockFreeXMLNode* PrevSibling() const {return previousSibling;}

	int childCount() {return m_childCount;}

	bool isLeaf() const { return firstChild == NULL; }
	void hide() { hidden = true; }
	int ancestorCount() const { return m_ancestorCount;}
	void write();
private:
	LockFreeXMLNode(const string& tagName, Outstream o);
	void Write(bool);

	Outstream out;
	AttributeList attributeList;

	AttributeStatus attrStatus;
	string tag;
	int m_childCount;
	int m_ancestorCount;

	LockFreeXMLNode* nextSibling;
	LockFreeXMLNode* previousSibling;
	LockFreeXMLNode* firstChild;
	// lastChild is not authoritative, it's just convenient and makes adding a new child O(1)
	LockFreeXMLNode* lastChild;
	LockFreeXMLNode* parent;

	bool hidden;
};




#endif /* LIB_PARALLELXML_LOCKFREEXMLNODE_HPP_ */
