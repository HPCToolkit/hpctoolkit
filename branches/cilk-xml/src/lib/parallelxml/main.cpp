/*
 * main.cpp
 *
 *  Created on: Dec 8, 2014
 *      Author: pat2
 */
#include <iostream>

#include "LockFreeXMLNode.hpp"
using std::cout;
int fib(LockFreeXMLNode* parent, int n);

string intToString(int a) {
	char buf[20];
	sprintf(buf, "%d", a);
	return string(buf);
}

int addFibNode(int n, LockFreeXMLNode* parent) {
	LockFreeXMLNode* fibNode = parent->createNode("fib");
	fibNode->AddAttribute(Attribute("n", intToString(n)));
	int val = fib(fibNode, n );
	fibNode->AddAttribute(Attribute("value", intToString(val)));
	fibNode->FinalizeAttributes();
	parent->AddChild(fibNode);
	return val;
}

int fib(LockFreeXMLNode* parent, int n) {
	if (n <= 1)
		return 1;
	int nm1 = cilk_spawn addFibNode(n-1, parent);
	int nm2 = cilk_spawn addFibNode(n-2, parent);
	cilk_sync;
	return nm1 + nm2;
}


int main(int argc, char** argv) {
	LockFreeXMLNode root("fib", &cout);
	root.AddAttribute(Attribute("n", "20"));
	int v = fib(&root, 20);
	root.AddAttribute(Attribute("value", intToString(v)));

	root.write();
	return 0;
}


