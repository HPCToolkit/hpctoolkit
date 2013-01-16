
#ifndef _JMT_H_
#define _JMT_H_

void * jmt_get_address(jmethodID method);

void jmt_add_java_method(jmethodID method, const void *address);

#endif //ifndef _JMT_H_
