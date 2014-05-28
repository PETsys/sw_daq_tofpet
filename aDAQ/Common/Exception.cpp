#include "Exception.hpp"
#include <stdio.h>
#include <string.h>

using namespace DAQ::Common;

Exception::Exception()
{
}
std::string Exception::getErrorString()
{
	return std::string(what());
}

bool Exception::isCritical()
{
	return true;
}

ExceptionCarrier::ExceptionCarrier(Exception & e)
{
	exception = e.clone();
}

ExceptionCarrier::ExceptionCarrier(const ExceptionCarrier & e)
{
	exception = e.exception->clone();
}

ExceptionCarrier * ExceptionCarrier::clone()
{
	return new ExceptionCarrier(*this);
}

void ExceptionCarrier::rethrow()
{
	exception->rethrow();
}

std::string ExceptionCarrier::getErrorString()
{
	return exception->getErrorString();
}

int ExceptionCarrier::getErrorCode()
{
	return exception->getErrorCode();
}

int ExceptionCarrier::getObjectID()
{
	return exception->getObjectID();
}

ExceptionCarrier::~ExceptionCarrier() throw()
{
	delete exception;
}

const char * ExceptionCarrier::what() const throw()
{
	return exception->what();
}

OSError::OSError(int err)
{
	errorCode = err;
}

const char * OSError::what() const throw()
{
	return "Operative system reported an error";
}


std::string OSError::getErrorString()
{
	char buffer[4096];
	sprintf(buffer, "%s: %ld (%s)", what(), (long int)errorCode, strerror(errorCode));
	return std::string(buffer);
}
