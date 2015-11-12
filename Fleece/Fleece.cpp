/*
 *  Fleece.cpp
 *  Fleece
 *
 *  Created by Jens Alfke on 11/12/15.
 *  Copyright © 2015 Couchbase. All rights reserved.
 *
 */

#include <iostream>
#include "Fleece.hpp"
#include "FleecePriv.hpp"

void Fleece::HelloWorld(const char * s)
{
	 FleecePriv *theObj = new FleecePriv;
	 theObj->HelloWorldPriv(s);
	 delete theObj;
};

void FleecePriv::HelloWorldPriv(const char * s) 
{
	std::cout << s << std::endl;
};

